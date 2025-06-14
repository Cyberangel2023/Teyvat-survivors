#include <graphics.h>
#include <iostream>
#include <tchar.h>
#include <string>
#include <vector>
#include <math.h>
#include <time.h>

using namespace std;

int idx_current_anim = 0;

const int PLAYER_ANIM_NUM = 6;

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

const int BUTTON_WIDTH = 192;
const int BUTTON_HEIGHT = 75;

bool is_game_start = false;
bool running = true;

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "MSIMG32.LIB")
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")

inline void putimage_alpha(int x, int y, IMAGE* img)
{
	int w = img->getwidth();
	int h = img->getheight();
	AlphaBlend(GetImageHDC(NULL), x, y, w, h,
		GetImageHDC(img), 0, 0, w, h, { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA });
}

class Atlas
{
public:
	Atlas(LPCTSTR path, int num)
	{
		TCHAR path_file[256];

		for (size_t i = 0; i < num; i++)
		{
			_stprintf_s(path_file, path, i);

			IMAGE* frame = new IMAGE();
			loadimage(frame, path_file);
			frame_file.push_back(frame);
		}
	}

	~Atlas()
	{
		for (size_t i = 0; i < frame_file.size(); i++)
			delete frame_file[i];
	}

public:
	vector<IMAGE*> frame_file;
};

Atlas* atlas_player_left;
Atlas* atlas_player_right;
Atlas* atlas_enemy_left;
Atlas* atlas_enemy_right;

class Animation
{
public:
	Animation(Atlas* atlas, int interval) //加载角色图片
	{
		anim_atlas = atlas;
		interval_ms = interval;
	}

	~Animation() = default;

	void Play(int x, int y, int delta) //角色动作演示
	{
		timer += delta;
		if (timer >= interval_ms)
		{
			idx_frame = (idx_frame + 1) % anim_atlas->frame_file.size();
			timer = 0;
		}

		putimage_alpha(x, y, anim_atlas->frame_file[idx_frame]);
	}

private:
	int timer = 0; //动画计时器
	int idx_frame = 0; //动画帧索引
	int interval_ms = 0;

private:
	Atlas* anim_atlas;
};

class Player
{
public:
	const int FRAME_WIDTH = 80;
	const int FRAME_HEIGHT = 80;
	POINT position = { 640, 360 };
	bool is_per_move_up = false;
	bool is_per_move_down = false;
	bool is_per_move_left = false;
	bool is_per_move_right = false;

public:
	Player() //玩家图片信息获取
	{
		loadimage(&image_shadow, _T("img/shadow_player.png"));
		anim_left = new Animation(atlas_player_left, 45);
		anim_right = new Animation(atlas_player_right, 45);
	}

	~Player()
	{
		delete anim_left;
		delete anim_right;
	}

	void ProcessEvent(const ExMessage& msg) //按键信息事件获取
	{
		switch (msg.message)
		{
		case WM_KEYDOWN:
			switch (msg.vkcode)
			{
			case VK_UP:
				is_per_move_up = true;
				break;
			case VK_DOWN:
				is_per_move_down = true;
				break;
			case VK_LEFT:
				is_per_move_left = true;
				break;
			case VK_RIGHT:
				is_per_move_right = true;
				break;
			}
			break;

		case WM_KEYUP:
			switch (msg.vkcode)
			{
			case VK_UP:
				is_per_move_up = false;
				break;
			case VK_DOWN:
				is_per_move_down = false;
				break;
			case VK_LEFT:
				is_per_move_left = false;
				break;
			case VK_RIGHT:
				is_per_move_right = false;
				break;
			}
			break;
		}
	}

	void Move() //玩家移动
	{
		int dir_x = is_per_move_right - is_per_move_left;
		int dir_y = is_per_move_down - is_per_move_up;
		double len_dir = sqrt(dir_x * dir_x + dir_y * dir_y);
		if (len_dir != 0)
		{
			double normalized_x = dir_x / len_dir;
			double normalized_y = dir_y / len_dir;
			position.x += (int)(SPEED * normalized_x);
			position.y += (int)(SPEED * normalized_y);
		}

		if (position.x < 0) position.x = 0;
		if (position.y < 0) position.y = 0;
		if (position.x + FRAME_WIDTH > WINDOW_WIDTH) position.x = WINDOW_WIDTH - FRAME_WIDTH;
		if (position.y + FRAME_HEIGHT > WINDOW_HEIGHT) position.y = WINDOW_HEIGHT - FRAME_HEIGHT;
	}

	void Draw(int delta) //玩家绘制
	{
		int pos_shadow_x = position.x + (FRAME_WIDTH / 2 - SHADOW_WIDTH / 2);
		int pos_shadow_y = position.y + FRAME_HEIGHT - 8;
		putimage_alpha(pos_shadow_x, pos_shadow_y, &image_shadow);

		static bool facing_left = false;
		int dir_x = is_per_move_right - is_per_move_left;
		if (dir_x < 0)
			facing_left = true;
		else if (dir_x > 0)
			facing_left = false;

		if (facing_left)
			anim_left->Play(position.x, position.y, delta);
		else
			anim_right->Play(position.x, position.y, delta);
	}

	const POINT& GetPosition() const
	{
		return position;
	}

private:
	const int SPEED = 3;
	const int SHADOW_WIDTH = 32;

private:
	IMAGE image_shadow;
	Animation* anim_left;
	Animation* anim_right;
};

class Bullet
{
public:
	POINT position = { 0,0 };

public:
	Bullet() = default;
	~Bullet() = default;

	void Draw() const //子弹绘制
	{
		setlinecolor(RGB(255, 155, 50));
		setfillcolor(RGB(200, 75, 10));
		fillcircle(position.x, position.y, RADIUS);
	}

private:
	const int RADIUS = 10;
};

class Enemy
{
public:
	Enemy() //敌人图片信息获取
	{
		loadimage(&image_shadow, _T("img/shadow_enemy.png"));
		anim_left = new Animation(atlas_enemy_left, 45);
		anim_right = new Animation(atlas_enemy_right, 45);

		//敌人生成边界
		enum class SpawnEdge
		{
			Up = 0,
			Down,
			Left,
			Right
		};

		//将敌人随机放置在边界外
		SpawnEdge edge = (SpawnEdge)(rand() % 4);
		switch (edge)
		{
		case SpawnEdge::Up:
			position.x = rand() % WINDOW_WIDTH;
			position.y = -FRAME_HEIGHT;
			break;
		case SpawnEdge::Down:
			position.x = rand() % WINDOW_WIDTH;
			position.y = WINDOW_HEIGHT;
			break;
		case SpawnEdge::Left:
			position.y = rand() % WINDOW_HEIGHT;
			position.x = -FRAME_WIDTH;
			break;
		case SpawnEdge::Right:
			position.y = rand() % WINDOW_HEIGHT;
			position.x = WINDOW_WIDTH;
			break;
		}
	}

	bool CheckBulletCollision(const Bullet& bullet) //检测与子弹碰撞
	{
		//将子弹等效成点，判断敌人是否在矩阵内
		bool is_overlap_x = bullet.position.x >= position.x && bullet.position.x <= position.x + FRAME_WIDTH;
		bool is_overlap_y = bullet.position.y >= position.y && bullet.position.y <= position.y + FRAME_HEIGHT;
		return is_overlap_x && is_overlap_y;
	}

	bool CheckPlayerCollision(const Player& player) //检测与玩家碰撞
	{
		//将敌人中心位置等效成点。判断点是否在玩家矩阵内
		POINT check_position = { position.x + FRAME_WIDTH / 2, position.y + FRAME_HEIGHT / 2 };
		bool is_overlap_x = player.GetPosition().x <= check_position.x && player.GetPosition().x + player.FRAME_WIDTH >= check_position.x;
		bool is_overlap_y = player.GetPosition().y <= check_position.y && player.GetPosition().y + player.FRAME_HEIGHT >= check_position.y;
		return is_overlap_x && is_overlap_y;
	}

	void Move(const Player& player) //敌人移动至玩家
	{
		const POINT& player_position = player.GetPosition();
		int dir_x = player_position.x - position.x;
		int dir_y = player_position.y - position.y;
		double len_dir = sqrt(dir_x * dir_x + dir_y * dir_y);
		if (len_dir != 0)
		{
			double normalized_x = dir_x / len_dir;
			double normalized_y = dir_y / len_dir;
			position.x += (int)(SPEED * normalized_x);
			position.y += (int)(SPEED * normalized_y);
		}

		if (dir_x < 0)
			facing_left = true;
		else if (dir_x > 0)
			facing_left = false;
	}

	void Draw(int delta) //敌人绘制
	{
		int pos_shadow_x = position.x + (FRAME_WIDTH / 2 - SHADOW_WIDTH / 2);
		int pos_shadow_y = position.y + FRAME_HEIGHT - 35;
		putimage_alpha(pos_shadow_x, pos_shadow_y, &image_shadow);

		if (facing_left)
			anim_left->Play(position.x, position.y, delta);
		else
			anim_right->Play(position.x, position.y, delta);
	}


	~Enemy()
	{
		delete anim_left;
		delete anim_right;
	}

	void Hurt()
	{
		alive = false;
	}

	bool CheckAlive()
	{
		return alive;
	}

private:
	const int SPEED = 2;
	const int FRAME_WIDTH = 80;
	const int FRAME_HEIGHT = 80;
	const int SHADOW_WIDTH = 48;

private:
	IMAGE image_shadow;
	Animation* anim_left;
	Animation* anim_right;
	POINT position = { 0, 0 };
	bool facing_left = false;
	bool alive = true;
};

class Button
{
public:
	Button(RECT rect, LPCTSTR path_img_idle, LPCTSTR path_img_hovered, LPCTSTR path_img_pushed)
	{
		region = rect;

		loadimage(&img_idgle, path_img_idle);
		loadimage(&img_hovered, path_img_hovered);
		loadimage(&img_pushed, path_img_pushed);
	}

	~Button() = default;

	void Draw()
	{
		switch (status)
		{
		case Status::Idgle:
			putimage(region.left, region.top, &img_idgle);
			break;
		case Status::Hovered:
			putimage(region.left, region.top, &img_hovered);
			break;
		case Status::Pushed:
			putimage(region.left, region.top, &img_pushed);
			break;
		}
	}

	void ProcessEvent(const ExMessage& msg)
	{
		switch (msg.message)
		{
		case WM_MOUSEMOVE:
			if (status == Status::Idgle && CheckCursorHit(msg.x, msg.y))
				status = Status::Hovered;
			else if (status == Status::Hovered && !CheckCursorHit(msg.x, msg.y))
				status = Status::Idgle;
			break;
		case WM_LBUTTONDOWN:
			if (CheckCursorHit(msg.x, msg.y))
				status = Status::Pushed;
			break;
		case WM_LBUTTONUP:
			if (status == Status::Pushed)
				OnClick();
			break;
		default:
			break;
		}
	}

protected:
	virtual void OnClick() = 0;

public:
	enum class Status
	{
		Idgle = 0,
		Hovered,
		Pushed
	};

private:
	RECT region;
	IMAGE img_idgle;
	IMAGE img_hovered;
	IMAGE img_pushed;

private:
	bool CheckCursorHit(int x, int y)
	{
		return x >= region.left && x <= region.right && y >= region.top && y <= region.bottom;
	}

public:
	Status status = Status::Idgle;
};

//开始游戏按钮
class StartGameButton : public Button
{
public:
	StartGameButton(RECT rect, LPCTSTR path_img_idle, LPCTSTR path_img_hovered, LPCTSTR path_img_pushed)
		: Button(rect, path_img_idle, path_img_hovered, path_img_pushed) {}

	~StartGameButton() = default;

protected:
	void OnClick()
	{
		is_game_start = true;

		mciSendString(_T("play bgm repeat from 0"), NULL, 0, NULL);
	}
};

//退出游戏按钮
class QuitGameButton : public Button
{
public:
	QuitGameButton(RECT rect, LPCTSTR path_img_idle, LPCTSTR path_img_hovered, LPCTSTR path_img_pushed)
		: Button(rect, path_img_idle, path_img_hovered, path_img_pushed) {}

	~QuitGameButton() = default;

protected:
	void OnClick()
	{
		running = false;
	}
};

//生成新的敌人
void TryGenerateEnemy(vector<Enemy*>& enemy_list)
{
	const int INTERVAL = 100;
	static int counter = 0;
	if ((++counter) % INTERVAL == 0)
		enemy_list.push_back(new Enemy());
}

//更新子弹位置
void UpdateBullets(vector<Bullet>& bullet_list, const Player& player)
{
	const double RADIAL_SPEED = 0.0045;                          //径向波动速度
	const double TANGENT_SPEED = 0.0055;                         //切向波动速度
	double radian_interval = 2 * 3.14159 / bullet_list.size();   //子弹之间的弧度间隔
	POINT player_position = player.GetPosition();
	double radius = 100 + 25 * sin(GetTickCount() * RADIAL_SPEED);
	for (size_t i = 0; i < bullet_list.size(); i++)
	{
		double radian = GetTickCount() * TANGENT_SPEED + radian_interval * i;   //当前子弹所在弧度值
		bullet_list[i].position.x = player_position.x + player.FRAME_WIDTH / 2 + (int)(radius * cos(radian));
		bullet_list[i].position.y = player_position.y + player.FRAME_HEIGHT / 2 + (int)(radius * sin(radian));
	}
}

//绘制玩家得分
void DrawPlayerScore(int score)
{
	static TCHAR text[64];
	_stprintf_s(text, _T("目前玩家得分：%d"), score);

	setbkmode(TRANSPARENT);
	settextcolor(RGB(255, 85, 185));
	outtextxy(10, 10, text);
}

int main()
{
	initgraph(1280, 720);

	atlas_player_left = new Atlas(_T("img/player_left_%d.png"), 6);
	atlas_player_right = new Atlas(_T("img/player_right_%d.png"), 6);
	atlas_enemy_left = new Atlas(_T("img/enemy_left_%d.png"), 6);
	atlas_enemy_right = new Atlas(_T("img/enemy_right_%d.png"), 6);

	mciSendString(_T("open mus/bgm.mp3 alias bgm"), NULL, 0, NULL);
	mciSendString(_T("open mus/hit.wav alias hit"), NULL, 0, NULL);

	int score = 0;
	Player player;
	ExMessage msg;
	IMAGE image_menu;
	IMAGE image_background;
	vector<Enemy*> enemy_list;
	vector<Bullet> bullet_list(3);

	RECT region_btn_start_game, region_btn_quit_game;

	region_btn_start_game.left = (WINDOW_WIDTH - BUTTON_WIDTH) / 2;
	region_btn_start_game.right = region_btn_start_game.left + BUTTON_WIDTH;
	region_btn_start_game.top = 430;
	region_btn_start_game.bottom = region_btn_start_game.top + BUTTON_HEIGHT;

	region_btn_quit_game.left = (WINDOW_WIDTH - BUTTON_WIDTH) / 2;
	region_btn_quit_game.right = region_btn_quit_game.left + BUTTON_WIDTH;
	region_btn_quit_game.top = 550;
	region_btn_quit_game.bottom = region_btn_quit_game.top + BUTTON_HEIGHT;

	StartGameButton btn_start_game = StartGameButton(region_btn_start_game, _T("img/ui_start_idle.png"),
		_T("img/ui_start_hovered.png"), _T("img/ui_start_pushed.png"));
	QuitGameButton btn_quit_game = QuitGameButton(region_btn_quit_game, _T("img/ui_quit_idle.png"),
		_T("img/ui_quit_hovered.png"), _T("img/ui_quit_pushed.png"));

	//加载游戏背景	
	loadimage(&image_menu, _T("img/menu.png"));
	loadimage(&image_background, _T("img/background.png"));

	BeginBatchDraw();

	while (running)
	{
		DWORD start_time = GetTickCount();

		while (peekmessage(&msg))
		{
			if (is_game_start)
				player.ProcessEvent(msg);
			else
			{
				btn_start_game.ProcessEvent(msg);
				btn_quit_game.ProcessEvent(msg);
			}
		}

		if (is_game_start)
		{

			//移动玩家
			player.Move();
			//更新子弹位置
			UpdateBullets(bullet_list, player);
			//尝试生成新的敌人
			TryGenerateEnemy(enemy_list);
			//更新敌人位置
			for (Enemy* enemy : enemy_list)
				enemy->Move(player);

			//检测敌人与子弹的碰撞
			for (Enemy* enemy : enemy_list)
			{
				for (const Bullet& bullet : bullet_list)
				{
					if (enemy->CheckBulletCollision(bullet))
					{
						mciSendString(_T("play hit from 0"), NULL, 0, NULL);
						enemy->Hurt();
						score++;
					}
				}
			}

			//检测敌人与玩家的碰撞
			for (Enemy* enemy : enemy_list)
			{
				if (enemy->CheckPlayerCollision(player))
				{
					static TCHAR text[64];
					MessageBox(GetHWnd(), _T("扣“1”观看战败CG！"), _T("派蒙五分之一野猪的战力打不过野猪！"), MB_OK);
					is_game_start = false;
					mciSendString(_T("pause bgm"), NULL, 0, NULL);

					score = 0;
					flushmessage();
					btn_start_game.status = Button::Status::Idgle;

					player.position = { 640, 360 };
					player.is_per_move_up = false;
					player.is_per_move_down = false;
					player.is_per_move_left = false;
					player.is_per_move_right = false;

					enemy_list.clear();

					break;
				}
			}

			//将生命值归零的敌人移除
			for (size_t i = 0; i < enemy_list.size(); i++)
			{
				Enemy* enemy = enemy_list[i];
				if (!enemy->CheckAlive())
				{
					swap(enemy_list.back(), enemy_list[i]);
					enemy_list.pop_back();
					delete enemy;
				}
			}
		}

		cleardevice();

		if (is_game_start)
		{
			putimage(0, 0, &image_background);
			player.Draw(1000 / 144);
			for (Enemy* enemy : enemy_list)
				enemy->Draw(1000 / 144);
			for (const Bullet& bullet : bullet_list)
				bullet.Draw();

			DrawPlayerScore(score);
		}
		else
		{
			putimage(0, 0, &image_menu);
			btn_start_game.Draw();
			btn_quit_game.Draw();
		}

		FlushBatchDraw();

		DWORD end_time = GetTickCount();
		DWORD delta_time = end_time - start_time;
		if (delta_time < 1000 / 144)
			Sleep(1000 / 144 - delta_time);
	}

	delete atlas_player_left;
	delete atlas_player_right;
	delete atlas_enemy_left;
	delete atlas_enemy_right;

	EndBatchDraw();

	return 0;
}
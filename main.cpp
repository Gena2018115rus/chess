#include <windows.h>
#include <fcntl.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <memory>
#include <numbers>
#include <cmath>
#include <vector>
#include <thread>
#include <mutex>
#include <regex>
#include "resource.h"
#include "eth-lib/eth-lib.hpp"

#define MAX_LOADSTRING 100

template <typename T>
struct coord_t : public std::pair<T, T>
{
    using std::pair<T, T>::pair;
    T& x() { return this->first; }
    T& y() { return this->second; }
    const T& x() const { return this->first; }
    const T& y() const { return this->second; }

    coord_t<T> operator +(const coord_t<T>& other) const;
    coord_t<T> operator -() const;
    
    template <typename U>
    coord_t<decltype(std::declval<T>() * std::declval<U>())>
    operator *(U n) const
    {
        return {x() * n, y() * n};
    }
    
    coord_t<T> operator -(T n) const {
        return {x() - n, y() - n};
    }
    
    coord_t<T> operator +(T n) const {
        return {x() + n, y() + n};
    }
    
    template <typename U>
    coord_t<decltype(std::declval<T>() / std::declval<U>())>
    operator /(const coord_t<U> &other) const {
        return {x() / other.x(), y() / other.y()};
    }
    
    template <typename U>
    operator coord_t<U>() const {
        return {x(), y()};
    }
};

template <typename T>
coord_t<T> coord_t<T>::operator+(const coord_t<T>& other) const
{
    return {{x() + other.x(), y() + other.y()}};
}

template <typename T>
coord_t<T> coord_t<T>::operator-() const
{
    return {{-x(), -y()}};
}

using icoord = coord_t<long long>;
using dcoord = coord_t<double>;

icoord d2icoord(dcoord dc);

HWND hWnd;
GLuint wolfw_texture, wolfm_texture,
       black_pawn_texture, white_pawn_texture,
       black_rook_texture, white_rook_texture,
       black_bishop_texture, white_bishop_texture,
       black_knight_texture, white_knight_texture,
       black_queen_texture, white_queen_texture,
       black_dragon_texture, white_dragon_texture,
       black_king_texture, white_king_texture;
HINSTANCE hInst;
icoord cursor;
std::mutex g_connection2host_mtx, g_client_mtx;
client_t *g_connection2host;
client_ref_t *g_client;
bool online_mode;
std::string in_buf;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

HWND createWindow(HINSTANCE hInstance, int nCmdShow, HACCEL *hAccelTable)
{
    WCHAR szTitle[MAX_LOADSTRING];				
    WCHAR szWindowClass[MAX_LOADSTRING];			
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PROJECT1, szWindowClass, MAX_LOADSTRING);

    WNDCLASSEXW wcexw =
    {
        sizeof(WNDCLASSEXW), 
        CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE,
        WndProc,
        0,
        0,
        hInstance,
        LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_PROJECT1)),
        LoadCursorW(NULL, IDC_ARROW),
        0, 
        0, 
        szWindowClass,
        LoadIconW(wcexw.hInstance, MAKEINTRESOURCEW(IDI_SMALL)) 
    };
    
    RegisterClassExW(&wcexw);
   
    HWND hWnd;  
    if (!(hWnd = CreateWindowExW(0L, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, 0, 0, 1366, 768, NULL, NULL, hInstance, NULL)))
    {
        std::wcerr << L"CreateWindowExW error" << std::endl;
        exit(0);
    }
    ShowWindow(hWnd, nCmdShow);
    *hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDC_PROJECT1)); 

    PIXELFORMATDESCRIPTOR pfd = {
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,
    24,
    0, 
    0,
    0,
    0,
    0,
    0,
    0, 
    0,
    0,
    0,
    0,
    0,
    0,
    32,
    0,
    0,
    0, 
    0,
    0,
    0,
    0,
    };

    int pixelFormat = ChoosePixelFormat(GetDC(hWnd), &pfd);
    if (SetPixelFormat(GetDC(hWnd), pixelFormat, &pfd) == 0)
    {
        fwprintf(stderr, L"Ошибка установки формата пикселей.");
        exit(-1);
    }

    wglMakeCurrent(GetDC(hWnd), wglCreateContext(GetDC(hWnd)));
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 

    return hWnd;
}

void LoadTexture(WORD IDB, GLuint *pTexture)
{
	HDC hdc = GetDC(hWnd);
	
	HBITMAP tmpRabbitBitmap = LoadBitmapW(hInst, MAKEINTRESOURCEW(IDB));
	SIZE tmpRabbitBitmap_size = {256, 256};
	DWORD bitmap_buffer_size = tmpRabbitBitmap_size.cx * tmpRabbitBitmap_size.cy * sizeof(DWORD);
	DWORD *bitmap_buffer = (DWORD *)malloc(bitmap_buffer_size);
	if (!bitmap_buffer)
	{
		fwprintf(stderr, L"Memory allocation error.\n");
		_wsystem(L"pause");
		exit(-1);
	}

	BITMAPINFO bitmap_info = {
		.bmiHeader = {
			.biSize = sizeof(bitmap_info.bmiHeader),
			.biWidth = tmpRabbitBitmap_size.cx,
			.biHeight = tmpRabbitBitmap_size.cy, 
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
			.biSizeImage = bitmap_buffer_size,
		},
	};
	if (!GetDIBits(hdc, tmpRabbitBitmap, 0, tmpRabbitBitmap_size.cy, bitmap_buffer, &bitmap_info, DIB_RGB_COLORS))
	{
		fwprintf(stderr, L"GetDIBits() error.\n");
		_wsystem(L"pause");
		exit(-2);
	}
	DeleteObject(tmpRabbitBitmap);


	glGenTextures(1, pTexture);
	glBindTexture(GL_TEXTURE_2D, *pTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		tmpRabbitBitmap_size.cx,
		tmpRabbitBitmap_size.cy,
		0,
		GL_BGRA_EXT,
		GL_UNSIGNED_BYTE,
		bitmap_buffer
	);
	free(bitmap_buffer); 

	glBindTexture(GL_TEXTURE_2D, 0);

	ReleaseDC(hWnd, hdc);

}

enum direction {
    RIGHT, UP, LEFT, DOWN 
} global_top = UP; 

enum piece_color {
    WHITE = 0, BLACK, NUMBER_OF_COLORS
};

struct square_t;
struct cell;
struct default_piece;

extern std::array<std::array<cell, 8>, 8> chessboard; 

std::shared_ptr<default_piece> selected_piece;
piece_color now_playing = WHITE;
icoord selected_from;

struct default_piece {
    piece_color color;
    inline static std::array<std::array<bool, 8>, 8> is_can_step_here_{};
    default_piece(piece_color color) : color(color) {
    }
    void draw(square_t place);
    virtual GLuint texture() const {
        switch (color) {
            case WHITE: return wolfw_texture;
            case BLACK: return wolfm_texture;
            default   : throw std::runtime_error("bad default_piece.color");
        }
    }
    virtual void show_where_can_step();
    virtual void moved() {
    };
    virtual void reset_where_can_step() {
        memset(is_can_step_here_.data(), 0, 8 * 8);
    }
    virtual std::array<std::array<bool, 8>, 8> &is_can_step_here() {
        return is_can_step_here_;
    }
    void ray_cast(const std::vector<icoord> &arr);
    virtual ~default_piece() {
    }
};

struct pawn : default_piece {
    bool is_first_step = true;
    inline static std::array<std::array<bool, 8>, 8> is_can_step_here_{};
    pawn(piece_color color) : default_piece(color) {
    }
    virtual GLuint texture() const override {
        switch (color) {
            case WHITE: return white_pawn_texture;
            case BLACK: return black_pawn_texture;
            default   : throw std::runtime_error("bad default_piece.color");
        }
    }
    virtual void show_where_can_step() override;
    virtual void moved() override {
        is_first_step = false;
    }
    virtual void reset_where_can_step() override {
        memset(is_can_step_here_.data(), 0, 8 * 8);
        default_piece::reset_where_can_step();
    }
    virtual std::array<std::array<bool, 8>, 8> &is_can_step_here() {
        return is_can_step_here_;
    }
};

struct dragon : default_piece {
    inline static std::array<std::array<bool, 8>, 8> is_can_step_here_{};
    dragon(piece_color color) : default_piece(color) {
    }
    virtual GLuint texture() const override {
        switch (color) {
            case WHITE: return white_dragon_texture;
            case BLACK: return black_dragon_texture;
            default   : throw std::runtime_error("bad default_piece.color");
        }
    }
    virtual void show_where_can_step() override;
    virtual void reset_where_can_step() override {
        memset(is_can_step_here_.data(), 0, 8 * 8);
        default_piece::reset_where_can_step();
    }
    virtual std::array<std::array<bool, 8>, 8> &is_can_step_here() {
        return is_can_step_here_;
    }
};

struct king : default_piece {
    inline static std::array<std::array<bool, 8>, 8> is_can_step_here_{};
    king(piece_color color) : default_piece(color) {
    }
    virtual GLuint texture() const override {
        switch (color) {
            case WHITE: return white_king_texture;
            case BLACK: return black_king_texture;
            default   : throw std::runtime_error("bad default_piece.color");
        }
    }
    virtual void show_where_can_step() override;
    virtual void reset_where_can_step() override {
        memset(is_can_step_here_.data(), 0, 8 * 8);
        default_piece::reset_where_can_step();
    }
    virtual std::array<std::array<bool, 8>, 8> &is_can_step_here() {
        return is_can_step_here_;
    }
};

struct knight : default_piece {
    inline static std::array<std::array<bool, 8>, 8> is_can_step_here_{};
    knight(piece_color color) : default_piece(color) {
    }
    virtual GLuint texture() const override {
        switch (color) {
            case WHITE: return white_knight_texture;
            case BLACK: return black_knight_texture;
            default   : throw std::runtime_error("bad default_piece.color");
        }
    }
    virtual void show_where_can_step() override;
    virtual void reset_where_can_step() override {
        memset(is_can_step_here_.data(), 0, 8 * 8);
        default_piece::reset_where_can_step();
    }
    virtual std::array<std::array<bool, 8>, 8> &is_can_step_here() {
        return is_can_step_here_;
    }
};

struct queen : default_piece {
    inline static std::array<std::array<bool, 8>, 8> is_can_step_here_{};
    queen(piece_color color) : default_piece(color) {
    }
    virtual GLuint texture() const override {
        switch (color) {
            case WHITE: return white_queen_texture;
            case BLACK: return black_queen_texture;
            default   : throw std::runtime_error("bad default_piece.color");
        }
    }
    virtual void show_where_can_step() override;
    virtual void reset_where_can_step() override {
        memset(is_can_step_here_.data(), 0, 8 * 8);
        default_piece::reset_where_can_step();
    }
    virtual std::array<std::array<bool, 8>, 8> &is_can_step_here() {
        return is_can_step_here_;
    }
};

struct bishop : default_piece {
    inline static std::array<std::array<bool, 8>, 8> is_can_step_here_{};
    bishop(piece_color color) : default_piece(color) {
    }
    virtual GLuint texture() const override {
        switch (color) {
            case WHITE: return white_bishop_texture;
            case BLACK: return black_bishop_texture;
            default   : throw std::runtime_error("bad default_piece.color");
        }
    }
    virtual void show_where_can_step() override;
    virtual void reset_where_can_step() override {
        memset(is_can_step_here_.data(), 0, 8 * 8);
        default_piece::reset_where_can_step();
    }
    virtual std::array<std::array<bool, 8>, 8> &is_can_step_here() {
        return is_can_step_here_;
    }
};

struct rook : default_piece {
    inline static std::array<std::array<bool, 8>, 8> is_can_step_here_{};
    rook(piece_color color) : default_piece(color) {
    }
    virtual GLuint texture() const override {
        switch (color) {
            case WHITE: return white_rook_texture;
            case BLACK: return black_rook_texture;
            default   : throw std::runtime_error("bad default_piece.color");
        }
    }
    virtual void show_where_can_step() override;
    virtual void reset_where_can_step() override {
        memset(is_can_step_here_.data(), 0, 8 * 8);
        default_piece::reset_where_can_step();
    }
    virtual std::array<std::array<bool, 8>, 8> &is_can_step_here() {
        return is_can_step_here_;
    }
};

void next_players_turn() {
    now_playing = (piece_color)((now_playing + 1) % NUMBER_OF_COLORS);
}

struct cell {
    std::shared_ptr<default_piece> piece;
    
    bool click(icoord self_coord) {
        if (selected_piece) {
            if (selected_piece->is_can_step_here()[self_coord.y()][self_coord.x()]) {
                piece = std::move(selected_piece);
                if (self_coord != selected_from) {
                    piece->moved();
                    next_players_turn();
                    return true;
                }
            }
        } else if (piece) {
            if (piece->color != now_playing)
                return false;
            selected_piece = std::move(piece);
            selected_from = self_coord;
            selected_piece->show_where_can_step();
        }
        return false;
    }
};

void default_piece::ray_cast(const std::vector<icoord> &arr) {
    for (auto [ix, iy] : arr) {
        for (auto [x, y] = selected_from;;) {
            x += ix, y += iy;
            try {
                if (default_piece::is_can_step_here().at(y).at(x)) {
                    is_can_step_here()[y][x] = true;
                    if (chessboard[y][x].piece) {
                        break;
                    }
                } else {
                    break;
                }
            } catch (const std::out_of_range &) {
                break;
            }
        }
    }
}

std::array<std::array<cell, 8>, 8> chessboard;

void default_piece::show_where_can_step() {
    reset_where_can_step();
    for (int y = 7; y >= 0; --y) {
        for (int x = 0; x < 8; ++x) {
            is_can_step_here_[y][x] = !chessboard[y][x].piece || color != chessboard[y][x].piece->color;
        }
    }
}

void pawn::show_where_can_step() {
    default_piece::show_where_can_step();

    int iDirection = color == WHITE ? 1 : -1;
    icoord forward1{selected_from.x(), selected_from.y() + iDirection},
           forward2{selected_from.x(), forward1.y() + iDirection},
           side1{forward1.x() + 1, forward1.y()},
           side2{forward1.x() - 1, forward1.y()};
    
    is_can_step_here_[selected_from.y()][selected_from.x()] = default_piece::is_can_step_here_[selected_from.y()][selected_from.x()];
    try {
        is_can_step_here_.at(forward1.y()).at(forward1.x()) = default_piece::is_can_step_here_.at(forward1.y()).at(forward1.x()) && !chessboard[forward1.y()][forward1.x()].piece;
    } catch (const std::out_of_range &) { }
    try {
        is_can_step_here_.at(forward2.y()).at(forward2.x()) = default_piece::is_can_step_here_.at(forward2.y()).at(forward2.x()) && !chessboard[forward1.y()][forward1.x()].piece && !chessboard[forward2.y()][forward2.x()].piece && is_first_step;
    } catch (const std::out_of_range &) { }
    try {
        is_can_step_here_.at(side1.y()).at(side1.x()) = default_piece::is_can_step_here_.at(side1.y()).at(side1.x()) && chessboard[side1.y()][side1.x()].piece && chessboard[side1.y()][side1.x()].piece->color != color;
    } catch (const std::out_of_range &) { }
    try {
        is_can_step_here_.at(side2.y()).at(side2.x()) = default_piece::is_can_step_here_.at(side2.y()).at(side2.x()) && chessboard[side2.y()][side2.x()].piece && chessboard[side2.y()][side2.x()].piece->color != color;
    } catch (const std::out_of_range &) { }
}

void dragon::show_where_can_step() {
    default_piece::show_where_can_step();
    
    is_can_step_here_[selected_from.y()][selected_from.x()] = default_piece::is_can_step_here_[selected_from.y()][selected_from.x()];
    for (auto [x, y] : std::to_array<icoord, 8>({{0, 2}, {1, 1}, {2, 0}, {1, -1}, {0, -2}, {-1, -1}, {-2, 0}, {-1, 1}})) {
        try {
            is_can_step_here_.at(selected_from.y() + y).at(selected_from.x() + x) = default_piece::is_can_step_here_.at(selected_from.y() + y).at(selected_from.x() + x);
        } catch (const std::out_of_range &) { }
    }
}

void king::show_where_can_step() {
    default_piece::show_where_can_step();
    
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            try {
                is_can_step_here_.at(selected_from.y() + y).at(selected_from.x() + x) = default_piece::is_can_step_here_.at(selected_from.y() + y).at(selected_from.x() + x);
            } catch (const std::out_of_range &) { }
        }
    }
}

void knight::show_where_can_step() {
    default_piece::show_where_can_step();
    
    is_can_step_here_[selected_from.y()][selected_from.x()] = default_piece::is_can_step_here_[selected_from.y()][selected_from.x()];
    for (auto [x, y] : std::to_array<icoord, 8>({{-1, -2}, {-2, -1}, {1, -2}, {2, -1}, {-1, 2}, {-2, 1}, {1, 2}, {2, 1}})) {
        try {
            is_can_step_here_.at(selected_from.y() + y).at(selected_from.x() + x) = default_piece::is_can_step_here_.at(selected_from.y() + y).at(selected_from.x() + x);
        } catch (const std::out_of_range &) { }
    }
}

void queen::show_where_can_step() {
    default_piece::show_where_can_step();
    
    is_can_step_here_[selected_from.y()][selected_from.x()] = default_piece::is_can_step_here_[selected_from.y()][selected_from.x()];
    ray_cast({{0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}});
}

void bishop::show_where_can_step() {
    default_piece::show_where_can_step();
    
    is_can_step_here_[selected_from.y()][selected_from.x()] = default_piece::is_can_step_here_[selected_from.y()][selected_from.x()];
    ray_cast({{1, 1}, {-1, -1}, {1, -1}, {-1, 1}});
}

void rook::show_where_can_step() {
    default_piece::show_where_can_step();
    
    is_can_step_here_[selected_from.y()][selected_from.x()] = default_piece::is_can_step_here_[selected_from.y()][selected_from.x()];
    ray_cast({{0, 1}, {0, -1}, {1, 0}, {-1, 0}});
}

std::shared_ptr<default_piece> make_piece(char c) {
    piece_color color = std::isupper(c) ? WHITE : BLACK;
    switch (std::toupper(c)) {
        case 'D': return std::make_shared<default_piece>(color);
        case ' ': return {};
        case 'P': return std::make_shared<pawn>(color);
        case 'K': return std::make_shared<king>(color);
        case 'H': return std::make_shared<knight>(color);
        case 'Q': return std::make_shared<queen>(color);
        case 'G': return std::make_shared<dragon>(color);
        case 'B': return std::make_shared<bishop>(color);
        case 'R': return std::make_shared<rook>(color);
        default : throw std::runtime_error("setup is ill formed");
    }
}

void init_chessboard () {
    static constexpr char setup[8][9] = { // 9 = 8 + 1; 1 for \0
 /* 8 */"rhbqkbhr", // Upper - white; Lower - black
 /* 7 */"pppppppp", // K - King
 /* 6 */"        ", // Q - Queen
 /* 5 */"        ", // R - Rook
 /* 4 */"        ", // B - Bishop
 /* 3 */"        ", // H - Knight (horse)
 /* 2 */"PPPPPPPP", // P - Pawn
 /* 1 */"RHBQKBHR"  // D - Default
    };/* abcdefgh *///   - Empty (space)
    
    for (std::size_t y = 0; y < 8; ++y) {
        for (std::size_t x = 0; x < 8; ++x) {
            chessboard[7 - y][x] = {make_piece(setup[y][x])};
        }
    }
}

void myClickEvent(icoord ic) {
    try {
        if (chessboard.at(ic.y()).at(ic.x()).click(ic) && online_mode)
        {
            auto line = std::to_string(selected_from.x()) + '&' + std::to_string(selected_from.y()) + '&' + std::to_string(ic.x()) + '&' + std::to_string(ic.y());
            g_client_mtx.lock();
            if (g_client)
            {
                line += "\r\n";
                g_client->write(line.data(), line.size());
            }
            g_client_mtx.unlock();
            g_connection2host_mtx.lock();
            // TODO: отлавливать отключение клиента и сервера (с двух сторон)
            if (g_connection2host)
            {
                g_connection2host->write(line.data(), line.size());
            }
            g_connection2host_mtx.unlock();
        }
    } catch (const std::out_of_range &) {
    }
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stdin ), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);


    HACCEL hAccelTable;
    hWnd = createWindow(hInstance, nCmdShow, &hAccelTable); 
    hInst = hInstance;

    LoadTexture(IDB_WOLFW, &wolfw_texture);
    LoadTexture(IDB_WOLFM, &wolfm_texture);
    LoadTexture(IDB_BLACK_PAWN  , &black_pawn_texture);
    LoadTexture(IDB_WHITE_PAWN  , &white_pawn_texture);
    LoadTexture(IDB_BLACK_ROOK  , &black_rook_texture);
    LoadTexture(IDB_WHITE_ROOK  , &white_rook_texture);
    LoadTexture(IDB_BLACK_BISHOP, &black_bishop_texture);
    LoadTexture(IDB_WHITE_BISHOP, &white_bishop_texture);
    LoadTexture(IDB_BLACK_KNIGHT, &black_knight_texture);
    LoadTexture(IDB_WHITE_KNIGHT, &white_knight_texture);
    LoadTexture(IDB_BLACK_QUEEN , &black_queen_texture);
    LoadTexture(IDB_WHITE_QUEEN , &white_queen_texture);
    LoadTexture(IDB_BLACK_DRAGON , &black_dragon_texture);
    LoadTexture(IDB_WHITE_DRAGON , &white_dragon_texture);
    LoadTexture(IDB_BLACK_KING  , &black_king_texture);
    LoadTexture(IDB_WHITE_KING  , &white_king_texture);

    
    init_chessboard();
    
    
    if (MessageBox(hWnd, L"Играть по сети?", L"Привет!", MB_YESNO) == IDYES)
    {
        online_mode = true;
        static std::regex re{"(-?\\d+)&(-?\\d+)&(-?\\d+)&(-?\\d+)"};
        if (MessageBox(hWnd, L"Быть хостом?", L"Ещё пара вопросов..", MB_YESNO) == IDYES)
        {
            // next_players_turn();
            std::thread([] {
                std::wcout << L"Запускаем сервер и ждём подключения..." << std::endl;
                make_listener(20182)->run([](client_ref_t client) {
                    g_client_mtx.lock();
                    if (!g_client)
                    {
                        static client_ref_t s_client = std::move(client);
                        g_client = &s_client;
                        std::wcout << L"Клиент подключился!" << std::endl;
                    }
                    
                    
                    g_client_mtx.unlock();
                }, [](const char *addr, const char *msg, size_t msg_sz) {
                    wprintf(L"Пришло от %s: %.*s\n", addr, msg_sz, msg);
                    std::string message{msg, msg + msg_sz};
                    std::smatch m;
                    if (std::regex_match(message, m, re)) {
                        wprintf(L"x1 = %s; y1 = %s; x2 = %s; y2 = %s;\n", m.str(1).data(), m.str(2).data(), m.str(3).data(), m.str(4).data());
                        myClickEvent({stoi(m[1]), stoi(m[2])});
                        myClickEvent({stoi(m[3]), stoi(m[4])});
                    }
                });
            }).detach();
        }
        else
        {
            std::thread([] {
                // std::wstring addr;
                // std::getline(std::wcin, addr);
                std::wcout << L"Пытаемся подключиться к " /*<< addr */<< L" ..." << std::endl;
                g_connection2host_mtx.lock();
                g_connection2host = make_client("localhost", "20182");
                g_connection2host_mtx.unlock();
                std::wcout << L"Соединение установлено!" << std::endl;
                g_connection2host->listen([](const char *chunk, size_t sz) {
                    wprintf(L"Пришло: %.*s", sz, chunk);
                    in_buf += {chunk, chunk + sz};
                    for (;;) {
                        auto index = in_buf.find("\r\n");
                        if (index != (size_t)-1) {
                            std::string line = {in_buf.begin(), in_buf.begin() + index};
                            in_buf = {in_buf.begin() + index + 2, in_buf.end()};
                            // обработка line
                            std::cout << "найдено: >>>" << line << "<<<" << std::endl;
                            std::smatch m;
                            if (std::regex_match(line, m, re)) {
                                wprintf(L"x1 = %s; y1 = %s; x2 = %s; y2 = %s;\n", m.str(1).data(), m.str(2).data(), m.str(3).data(), m.str(4).data());
                                myClickEvent({stoi(m[1]), stoi(m[2])});
                                myClickEvent({stoi(m[3]), stoi(m[4])});
                            }
                        } else {
                            break;
                        }
                    }
                });
            }).detach();
        }
    }
    

    for (MSG msg = {0}; msg.message != WM_QUIT; )
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) 
        {             
            if (!TranslateAcceleratorW(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg); 
            }
        }
        SendMessageW(hWnd, WM_TIMER, 0, 0);
    }



    return 0;
}

icoord getWindowWH() {
    RECT rect; 
    GetClientRect(hWnd, &rect);
   
    return {rect.right - rect.left, rect.bottom - rect.top};
}

struct square_t
{
    double x, y, w, h;
};

square_t getStartCoord() 
{
    square_t res;

    auto [w, h] = getWindowWH();

    if (w > h)
    {
        res.h = 2.0 / 8;
        res.w = 2.0 / 8 / ((double)w / h); 

    }
    else
    {
        res.h = 2.0 / 8 / ((double)h / w);
        res.w = 2.0 / 8;
 
    }
    

    if (w > h)
    {
        res.x = -1.0 + (2 - res.w * 8) / 2; 
        res.y = 1.0;
    }
    else
    {
        res.x = -1.0;
        res.y = 1.0 - (2 - res.h * 8) / 2;
    }


    return res;  
}

icoord d2icoord(dcoord dc) {
    auto [x, y, w, h] = getStartCoord();
    return (dc / dcoord{-x, y} + 1) * 4;
}

template <typename T>
T getCursorCoord();

template <>
dcoord getCursorCoord<dcoord>() {
    return (cursor * 2.0 / getWindowWH() - 1) / dcoord{1, -1};
}

template <>
icoord getCursorCoord<icoord>() {
    return d2icoord(getCursorCoord<dcoord>());
}


void default_piece::draw(square_t place) {
    
    glColor3d(1.0, 1.0, 1.0); 
    glBindTexture(GL_TEXTURE_2D, texture());
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0); glVertex2d(place.x, place.y);
    glTexCoord2i(1, 0); glVertex2d(place.x + place.w, place.y);
    glTexCoord2i(1, 1); glVertex2d(place.x + place.w, place.y + place.h);
    glTexCoord2i(0, 1); glVertex2d(place.x, place.y + place.h);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0); 
}


void draw_circle(dcoord center, double radius) {

    auto [x, y, w, h] = getStartCoord();
    glBegin(GL_TRIANGLE_FAN);
    glVertex2d(center.x(), center.y());
    for (std::size_t i = 0; i <= 120; ++i)
        glVertex2d(center.x() + radius * w * std::cos(std::numbers::pi * 2 * i / 120),
                   center.y() + radius * h * std::sin(std::numbers::pi * 2 * i / 120));
    glEnd();
}

void draw()
{
    auto [x, y, w, h] = getStartCoord();

    bool flag = false;
    for (int iy = 0; iy < 8; ++iy)
    {
        for (int ix = 0; ix < 8; ++ix)
        {
            if (flag ^= 1) glColor3d(1.0, 1.0, 1.0);
            else glColor3d(0.0, 0.0, 0.0);
            square_t place{x + w * ix, y - h * (iy + 1), w, h};
            glRectd(place.x, place.y, place.x + w, place.y + h);
        }
        flag ^= 1;
    }
    
    for (int iy = 0; iy < 8; ++iy)
    {
        for (int ix = 0; ix < 8; ++ix)
        {
            square_t place;
            if (online_mode && g_client) {
                place = {x + w * (7 - ix), y - h * (7 - iy + 1), w, h};
            } else {
                place = {x + w * ix, y - h * (iy + 1), w, h};
            }
            if (chessboard[7 - iy][ix].piece)
                chessboard[7 - iy][ix].piece->draw(place);
            if (selected_piece && selected_piece->is_can_step_here()[7 - iy][ix]) {
                glColor3d(0.0, 0.5, 0.0);
                draw_circle({place.x + place.w / 2, place.y + place.h / 2}, 0.125);
            }
        }
    }
    
    if (selected_piece) {
        dcoord cur = getCursorCoord<dcoord>();
        selected_piece->draw({cur.x() - w / 2, cur.y() - h / 2, w, h});
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    switch (message)
    {
        case WM_MOUSEMOVE:
        {
        	cursor = {LOWORD(lParam), HIWORD(lParam)}; 
        }
        break;
        case WM_LBUTTONUP:
        {
            if (online_mode && now_playing != (g_client ? BLACK : g_connection2host ? WHITE : NUMBER_OF_COLORS)) { // сюда бы мьютекс, чтоб уж соответствовать
                break;
            }
            icoord ic = getCursorCoord<icoord>();
            if (online_mode && g_client) {
                ic.x() = 7 - ic.x();
                ic.y() = 7 - ic.y();
            }
            myClickEvent(ic);
        }
        break;
        case WM_SIZE:
        {
            glViewport(0, 0, LOWORD(lParam), HIWORD(lParam));  
        }
        break;
        case WM_PAINT: 
        {
            PAINTSTRUCT ps;
        	HDC hdc = BeginPaint(hWnd, &ps); 
        	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        	glEnable(GL_TEXTURE_2D);
        	glEnable(GL_BLEND);
        	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


            draw();


        	glDisable(GL_BLEND);
        	glDisable(GL_TEXTURE_2D);
        	glBindTexture(GL_TEXTURE_2D, 0);
        	glFlush();
        	SwapBuffers(hdc); 
        	EndPaint(hWnd, &ps);
        }
        break;
        case WM_TIMER:
        {

            RECT rect;
        	GetClientRect(hWnd, &rect);
        	InvalidateRect(hWnd, &rect, TRUE); 
        	UpdateWindow(hWnd);
        }
        break;
        case WM_DESTROY:
        {

            PostQuitMessage(0);
        }
        break;
        default: return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
} 
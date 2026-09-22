#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <sstream>

// ─── Constants ────────────────────────────────────────────────────────────────
const int   WIN_W        = 800;
const int   WIN_H        = 600;
const float PADDLE_W     = 110.f;
const float PADDLE_H     = 14.f;
const float PADDLE_SPD   = 520.f;
const float BALL_RADIUS  = 9.f;
const float BALL_SPD     = 380.f;
const int   BRICK_COLS   = 10;
const int   BRICK_ROWS   = 5;
const float BRICK_W      = 68.f;
const float BRICK_H      = 22.f;
const float BRICK_PAD    = 6.f;
const float BRICK_OFF_X  = 26.f;
const float BRICK_OFF_Y  = 60.f;
const float BULLET_W     = 5.f;
const float BULLET_H     = 13.f;
const float BULLET_SPD   = 220.f;
const float BULLET_INTERVAL = 2.8f;   // seconds between shots per brick
const int   PADDLE_MAX_HP = 5;

// ─── Color Palette ────────────────────────────────────────────────────────────
const sf::Color COL_BG       (15,  15,  30);
const sf::Color COL_PADDLE   (100, 200, 255);
const sf::Color COL_BALL     (255, 240, 100);
const sf::Color COL_BULLET   (255,  80,  80);
const sf::Color COL_HP_FULL  ( 80, 210,  80);
const sf::Color COL_HP_LOW   (220,  60,  60);
const sf::Color COL_SCORE    (255, 255, 255);
const sf::Color COL_OVERLAY  (0,   0,   0,  170);

const sf::Color BRICK_COLORS[BRICK_ROWS] = {
    {220,  60,  80},
    {220, 140,  40},
    {180, 220,  40},
    { 40, 180, 220},
    {160,  60, 220}
};

// ─── Game State ───────────────────────────────────────────────────────────────
enum class State { START, PLAYING, PAUSED, GAME_OVER, WIN };

// ─── Structs ──────────────────────────────────────────────────────────────────
struct Brick {
    sf::RectangleShape shape;
    bool alive   = true;
    float timer  = 0.f;       // countdown to next bullet
    float delay  = 0.f;       // initial stagger delay
};

struct Bullet {
    sf::RectangleShape shape;
    bool alive = true;
};

// ─── Helpers ──────────────────────────────────────────────────────────────────
static sf::Text makeText(const sf::Font& font, const std::string& str,
                         unsigned size, sf::Color col = sf::Color::White) {
    sf::Text t;
    t.setFont(font);
    t.setString(str);
    t.setCharacterSize(size);
    t.setFillColor(col);
    return t;
}

static void centre(sf::Text& t, float x, float y) {
    sf::FloatRect r = t.getLocalBounds();
    t.setOrigin(r.left + r.width / 2.f, r.top + r.height / 2.f);
    t.setPosition(x, y);
}

// ─── Game ─────────────────────────────────────────────────────────────────────
class Game {
public:
    Game() : window(sf::VideoMode(WIN_W, WIN_H), "BrickBreaker",
                    sf::Style::Titlebar | sf::Style::Close)
    {
        window.setFramerateLimit(60);
        std::srand(static_cast<unsigned>(std::time(nullptr)));

        // Load a system font fallback
        // Try several paths common on Linux/Ubuntu
        const char* fontPaths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
        };
        bool loaded = false;
        for (auto p : fontPaths) {
            if (font.loadFromFile(p)) { loaded = true; break; }
        }
        if (!loaded) {
            // Create a minimal fallback — text won't render but game runs
        }

        // Build paddle
        paddle.setSize({PADDLE_W, PADDLE_H});
        paddle.setFillColor(COL_PADDLE);
        paddle.setOrigin(PADDLE_W / 2.f, PADDLE_H / 2.f);

        // Health bar background
        hpBarBg.setSize({PADDLE_W, 8.f});
        hpBarBg.setFillColor(sf::Color(60, 60, 60));
        hpBarBg.setOrigin(PADDLE_W / 2.f, 0.f);

        // Ball
        ball.setRadius(BALL_RADIUS);
        ball.setFillColor(COL_BALL);
        ball.setOrigin(BALL_RADIUS, BALL_RADIUS);

        resetGame();
    }

    void run() {
        sf::Clock clock;
        while (window.isOpen()) {
            float dt = clock.restart().asSeconds();
            dt = std::min(dt, 0.05f); // clamp to avoid huge steps

            handleEvents();
            if (state == State::PLAYING) update(dt);
            render();
        }
    }

private:
    sf::RenderWindow   window;
    sf::Font           font;
    State              state = State::START;

    // Paddle
    sf::RectangleShape paddle;
    int                paddleHp = PADDLE_MAX_HP;
    sf::RectangleShape hpBarBg;
    sf::RectangleShape hpBar;

    // Ball
    sf::CircleShape    ball;
    sf::Vector2f       ballVel;
    bool               ballLaunched = false;

    // Bricks & bullets
    std::vector<Brick>  bricks;
    std::vector<Bullet> bullets;

    int  score    = 0;
    int  hiScore  = 0;

    // ── Reset ──────────────────────────────────────────────────────────────
    void resetGame() {
        paddle.setPosition(WIN_W / 2.f, WIN_H - 50.f);
        paddleHp = PADDLE_MAX_HP;
        ballLaunched = false;
        ball.setPosition(WIN_W / 2.f, WIN_H - 70.f);
        ballVel = {0.f, 0.f};
        bullets.clear();
        buildBricks();
        score = 0;
        state = State::PLAYING;
    }

    void buildBricks() {
        bricks.clear();
        for (int r = 0; r < BRICK_ROWS; ++r) {
            for (int c = 0; c < BRICK_COLS; ++c) {
                Brick b;
                float x = BRICK_OFF_X + c * (BRICK_W + BRICK_PAD);
                float y = BRICK_OFF_Y + r * (BRICK_H + BRICK_PAD);
                b.shape.setSize({BRICK_W, BRICK_H});
                b.shape.setPosition(x, y);
                b.shape.setFillColor(BRICK_COLORS[r]);
                // Add a subtle outline
                b.shape.setOutlineThickness(1.5f);
                b.shape.setOutlineColor(sf::Color(0, 0, 0, 80));
                // Stagger their shoot timers so not all fire at once
                b.delay = static_cast<float>(std::rand() % 280) / 100.f;
                b.timer = BULLET_INTERVAL + b.delay;
                bricks.push_back(b);
            }
        }
    }

    // ── Events ─────────────────────────────────────────────────────────────
    void handleEvents() {
        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed) window.close();

            if (ev.type == sf::Event::KeyPressed) {
                switch (state) {
                case State::START:
                    if (ev.key.code == sf::Keyboard::Enter ||
                        ev.key.code == sf::Keyboard::Space)
                        resetGame();
                    break;

                case State::PLAYING:
                    if (ev.key.code == sf::Keyboard::Escape ||
                        ev.key.code == sf::Keyboard::P)
                        state = State::PAUSED;
                    if (!ballLaunched &&
                        (ev.key.code == sf::Keyboard::Space ||
                         ev.key.code == sf::Keyboard::Up))
                        launchBall();
                    break;

                case State::PAUSED:
                    if (ev.key.code == sf::Keyboard::Escape ||
                        ev.key.code == sf::Keyboard::P)
                        state = State::PLAYING;
                    if (ev.key.code == sf::Keyboard::R)
                        resetGame();
                    if (ev.key.code == sf::Keyboard::M) {
                        state = State::START;
                    }
                    break;

                case State::GAME_OVER:
                case State::WIN:
                    if (ev.key.code == sf::Keyboard::Enter ||
                        ev.key.code == sf::Keyboard::Space)
                        resetGame();
                    if (ev.key.code == sf::Keyboard::M)
                        state = State::START;
                    break;

                default: break;
                }
            }
        }
    }

    void launchBall() {
        ballLaunched = true;
        // Random upward angle
        float angle = -70.f + static_cast<float>(std::rand() % 40); // -70 to -30 deg
        float rad   = angle * 3.14159f / 180.f;
        ballVel     = {BALL_SPD * std::sin(rad), -std::abs(BALL_SPD * std::cos(rad))};
    }

    // ── Update ─────────────────────────────────────────────────────────────
    void update(float dt) {
        movePaddle(dt);
        if (!ballLaunched) {
            // Ball sits on paddle
            ball.setPosition(paddle.getPosition().x,
                             paddle.getPosition().y - PADDLE_H / 2.f - BALL_RADIUS - 2.f);
        } else {
            moveBall(dt);
        }
        updateBricks(dt);
        updateBullets(dt);
        checkWin();
    }

    void movePaddle(float dt) {
        float px = paddle.getPosition().x;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)  ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::A))
            px -= PADDLE_SPD * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::D))
            px += PADDLE_SPD * dt;
        px = std::max(PADDLE_W / 2.f, std::min(WIN_W - PADDLE_W / 2.f, px));
        paddle.setPosition(px, WIN_H - 50.f);
    }

    void moveBall(float dt) {
        sf::Vector2f pos = ball.getPosition();
        pos += ballVel * dt;

        // Wall bounces
        if (pos.x - BALL_RADIUS < 0.f)  { pos.x = BALL_RADIUS;        ballVel.x =  std::abs(ballVel.x); }
        if (pos.x + BALL_RADIUS > WIN_W) { pos.x = WIN_W - BALL_RADIUS; ballVel.x = -std::abs(ballVel.x); }
        if (pos.y - BALL_RADIUS < 0.f)  { pos.y = BALL_RADIUS;         ballVel.y =  std::abs(ballVel.y); }

        // Ball lost
        if (pos.y - BALL_RADIUS > WIN_H) {
            hiScore = std::max(hiScore, score);
            state   = State::GAME_OVER;
            return;
        }

        // Paddle collision
        sf::FloatRect pRect = paddle.getGlobalBounds();
        if (ballVel.y > 0.f &&
            pos.x > pRect.left && pos.x < pRect.left + pRect.width &&
            pos.y + BALL_RADIUS >= pRect.top &&
            pos.y + BALL_RADIUS <= pRect.top + pRect.height + 10.f)
        {
            ballVel.y = -std::abs(ballVel.y);
            // Offset x velocity based on hit position for better control
            float offset = (pos.x - paddle.getPosition().x) / (PADDLE_W / 2.f);
            ballVel.x    = offset * BALL_SPD;
            // Keep speed constant
            float spd    = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
            ballVel      = ballVel / spd * BALL_SPD;
            pos.y        = pRect.top - BALL_RADIUS;
        }

        // Brick collisions
        sf::FloatRect ballRect(pos.x - BALL_RADIUS, pos.y - BALL_RADIUS,
                               BALL_RADIUS * 2.f, BALL_RADIUS * 2.f);
        for (auto& b : bricks) {
            if (!b.alive) continue;
            sf::FloatRect br = b.shape.getGlobalBounds();
            if (ballRect.intersects(br)) {
                b.alive = false;
                score += 10;

                // Determine bounce axis
                float overlapL = (ballRect.left + ballRect.width) - br.left;
                float overlapR = (br.left + br.width) - ballRect.left;
                float overlapT = (ballRect.top + ballRect.height) - br.top;
                float overlapB = (br.top + br.height) - ballRect.top;
                float minH = std::min(overlapL, overlapR);
                float minV = std::min(overlapT, overlapB);
                if (minH < minV) ballVel.x = -ballVel.x;
                else             ballVel.y = -ballVel.y;
                break;
            }
        }

        ball.setPosition(pos);
    }

    void updateBricks(float dt) {
        for (auto& b : bricks) {
            if (!b.alive) continue;
            b.timer -= dt;
            if (b.timer <= 0.f) {
                b.timer = BULLET_INTERVAL;
                spawnBullet(b.shape.getPosition().x + BRICK_W / 2.f,
                            b.shape.getPosition().y + BRICK_H);
            }
        }
    }

    void spawnBullet(float x, float y) {
        Bullet bl;
        bl.shape.setSize({BULLET_W, BULLET_H});
        bl.shape.setFillColor(COL_BULLET);
        bl.shape.setOrigin(BULLET_W / 2.f, 0.f);
        bl.shape.setPosition(x, y);
        bullets.push_back(bl);
    }

    void updateBullets(float dt) {
        sf::FloatRect pRect = paddle.getGlobalBounds();
        for (auto& bl : bullets) {
            if (!bl.alive) continue;
            bl.shape.move(0.f, BULLET_SPD * dt);

            // Off screen
            if (bl.shape.getPosition().y > WIN_H) { bl.alive = false; continue; }

            // Hit paddle
            if (bl.shape.getGlobalBounds().intersects(pRect)) {
                bl.alive = false;
                paddleHp--;
                if (paddleHp <= 0) {
                    hiScore = std::max(hiScore, score);
                    state   = State::GAME_OVER;
                }
            }
        }
        // Prune dead bullets
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                      [](const Bullet& b){ return !b.alive; }), bullets.end());
    }

    void checkWin() {
        bool anyAlive = false;
        for (auto& b : bricks) if (b.alive) { anyAlive = true; break; }
        if (!anyAlive) {
            hiScore = std::max(hiScore, score);
            state   = State::WIN;
        }
    }

    // ── Render ─────────────────────────────────────────────────────────────
    void render() {
        window.clear(COL_BG);

        switch (state) {
        case State::START:     drawStart();   break;
        case State::PLAYING:   drawGame();    break;
        case State::PAUSED:    drawGame(); drawPause();   break;
        case State::GAME_OVER: drawGame(); drawGameOver(); break;
        case State::WIN:       drawGame(); drawWin();     break;
        }

        window.display();
    }

    void drawGame() {
        // Bricks
        for (auto& b : bricks)
            if (b.alive) window.draw(b.shape);

        // Bullets
        for (auto& bl : bullets)
            if (bl.alive) window.draw(bl.shape);

        // Ball
        window.draw(ball);

        // Paddle
        window.draw(paddle);

        // Health bar (above paddle)
        float hpFrac = static_cast<float>(paddleHp) / PADDLE_MAX_HP;
        sf::Vector2f pPos = paddle.getPosition();
        hpBarBg.setPosition(pPos.x, pPos.y - PADDLE_H / 2.f - 14.f);
        window.draw(hpBarBg);

        sf::RectangleShape hpBar;
        hpBar.setSize({PADDLE_W * hpFrac, 8.f});
        hpBar.setFillColor(hpFrac > 0.4f ? COL_HP_FULL : COL_HP_LOW);
        hpBar.setOrigin(PADDLE_W / 2.f, 0.f);
        hpBar.setPosition(pPos.x, pPos.y - PADDLE_H / 2.f - 14.f);
        window.draw(hpBar);

        // Heart icons (HP pips)
        for (int i = 0; i < PADDLE_MAX_HP; ++i) {
            sf::RectangleShape pip({8.f, 8.f});
            pip.setFillColor(i < paddleHp ? COL_HP_FULL : sf::Color(60,60,60));
            pip.setPosition(10.f + i * 14.f, WIN_H - 20.f);
            window.draw(pip);
        }

        // Score
        auto scoreText = makeText(font, "SCORE  " + std::to_string(score), 20, COL_SCORE);
        scoreText.setPosition(WIN_W / 2.f - scoreText.getLocalBounds().width / 2.f, 10.f);
        window.draw(scoreText);

        auto hiText = makeText(font, "BEST  " + std::to_string(hiScore), 16, sf::Color(180,180,180));
        hiText.setPosition(WIN_W - hiText.getLocalBounds().width - 10.f, 12.f);
        window.draw(hiText);

        // Controls hint (small)
        if (!ballLaunched && state == State::PLAYING) {
            auto hint = makeText(font, "SPACE to launch ball", 16, sf::Color(160,160,200));
            centre(hint, WIN_W / 2.f, WIN_H - 20.f);
            window.draw(hint);
        }
    }

    void drawOverlay(sf::Color col = COL_OVERLAY) {
        sf::RectangleShape overlay({(float)WIN_W, (float)WIN_H});
        overlay.setFillColor(col);
        window.draw(overlay);
    }

    void drawStart() {
        // Background gradient effect (layered rects)
        for (int i = 0; i < 6; ++i) {
            sf::RectangleShape row({(float)WIN_W, WIN_H / 6.f});
            row.setPosition(0.f, i * WIN_H / 6.f);
            row.setFillColor(sf::Color(15, 15, 30 + i * 8));
            window.draw(row);
        }

        // Decorative bricks
        for (int c = 0; c < 10; ++c) {
            sf::RectangleShape deco({BRICK_W, BRICK_H});
            deco.setPosition(BRICK_OFF_X + c * (BRICK_W + BRICK_PAD), 80.f);
            deco.setFillColor(BRICK_COLORS[c % BRICK_ROWS]);
            deco.setOutlineThickness(1.5f);
            deco.setOutlineColor(sf::Color(0,0,0,80));
            window.draw(deco);
        }

        auto title = makeText(font, "BRICK", 72, sf::Color(100, 200, 255));
        title.setStyle(sf::Text::Bold);
        centre(title, WIN_W / 2.f - 90.f, 200.f);
        window.draw(title);

        auto title2 = makeText(font, "BREAKER", 72, sf::Color(255, 240, 100));
        title2.setStyle(sf::Text::Bold);
        centre(title2, WIN_W / 2.f + 100.f, 200.f);
        window.draw(title2);

        auto sub = makeText(font, "Survive the bullet storm!", 22, sf::Color(200, 200, 220));
        centre(sub, WIN_W / 2.f, 270.f);
        window.draw(sub);

        drawMenuBox(340.f, 420.f, {
            {"PRESS  SPACE  OR  ENTER  TO  PLAY", sf::Color(100,255,150)},
        });

        auto ctrl = makeText(font, "A/D or LEFT/RIGHT  to move     P or ESC  to pause", 15,
                             sf::Color(140,140,160));
        centre(ctrl, WIN_W / 2.f, 520.f);
        window.draw(ctrl);

        auto ctrl2 = makeText(font, "Dodge enemy bullets!  Don't lose the ball!", 15,
                              sf::Color(255, 120, 120));
        centre(ctrl2, WIN_W / 2.f, 548.f);
        window.draw(ctrl2);

        if (hiScore > 0) {
            auto hi = makeText(font, "BEST SCORE:  " + std::to_string(hiScore), 18,
                               sf::Color(255, 220, 80));
            centre(hi, WIN_W / 2.f, 480.f);
            window.draw(hi);
        }
    }

    void drawPause() {
        drawOverlay();
        auto title = makeText(font, "PAUSED", 56, sf::Color(100, 200, 255));
        title.setStyle(sf::Text::Bold);
        centre(title, WIN_W / 2.f, 200.f);
        window.draw(title);

        drawMenuBox(WIN_H / 2.f, WIN_H / 2.f + 80.f, {
            {"P / ESC   -   Resume",      sf::Color(255,255,255)},
            {"R         -   Restart",     sf::Color(200,200,200)},
            {"M         -   Main Menu",   sf::Color(200,200,200)},
        });
    }

    void drawGameOver() {
        drawOverlay();
        auto title = makeText(font, "GAME OVER", 56, sf::Color(255, 80, 80));
        title.setStyle(sf::Text::Bold);
        centre(title, WIN_W / 2.f, 170.f);
        window.draw(title);

        auto sc = makeText(font, "Score:  " + std::to_string(score), 28, sf::Color(255,255,255));
        centre(sc, WIN_W / 2.f, 250.f);
        window.draw(sc);

        auto hi = makeText(font, "Best:  " + std::to_string(hiScore), 22, sf::Color(255,220,80));
        centre(hi, WIN_W / 2.f, 290.f);
        window.draw(hi);

        drawMenuBox(350.f, WIN_H / 2.f + 90.f, {
            {"SPACE / ENTER   -   Play Again", sf::Color(100, 255, 150)},
            {"M               -   Main Menu",  sf::Color(200, 200, 200)},
        });
    }

    void drawWin() {
        drawOverlay(sf::Color(0, 30, 0, 180));
        auto title = makeText(font, "YOU WIN!", 60, sf::Color(100, 255, 130));
        title.setStyle(sf::Text::Bold);
        centre(title, WIN_W / 2.f, 170.f);
        window.draw(title);

        auto sc = makeText(font, "Score:  " + std::to_string(score), 28, sf::Color(255,255,255));
        centre(sc, WIN_W / 2.f, 260.f);
        window.draw(sc);

        auto hi = makeText(font, "Best:  " + std::to_string(hiScore), 22, sf::Color(255,220,80));
        centre(hi, WIN_W / 2.f, 298.f);
        window.draw(hi);

        drawMenuBox(360.f, WIN_H / 2.f + 100.f, {
            {"SPACE / ENTER   -   Play Again", sf::Color(100, 255, 150)},
            {"M               -   Main Menu",  sf::Color(200, 200, 200)},
        });
    }

    // Helper: draws a tidy centred menu box
    void drawMenuBox(float topY, float /*bottomY*/,
                     std::vector<std::pair<std::string, sf::Color>> items)
    {
        float y = topY;
        float lineH = 40.f;
        for (auto& [str, col] : items) {
            auto t = makeText(font, str, 22, col);
            centre(t, WIN_W / 2.f, y);
            window.draw(t);
            y += lineH;
        }
    }
};

// ─── Entry Point ──────────────────────────────────────────────────────────────
int main() {
    Game game;
    game.run();
    return 0;
}

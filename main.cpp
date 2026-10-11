// ================================================================
//  B R I C K B R E A K E R  -  SFML 3.x  -  C++17
//
//  Visual Studio setup:
//    C/C++ > Language > C++ Language Standard = C++17
//    Linker > Input > Additional Dependencies:
//      sfml-graphics.lib;sfml-window.lib;sfml-system.lib
//
//  GCC/Clang:
//    g++ -std=c++17 -O2 main.cpp -lsfml-graphics -lsfml-window -lsfml-system
// ================================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include <array>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <optional>
#include <initializer_list>

constexpr float PI = 3.14159265358979f;

// Brick colour/type -- determines points AND bullet behaviour.
// Declared up-front so the per-stage config table below can reference it.
enum class BrickType { RED = 0, ORANGE = 1, GREEN = 2, BLUE = 3, PURPLE = 4, COUNT = 5 };

// ================================================================
//  ***  EASY-EDIT CONFIGURATION  ***
//  Everything in this section is meant to be tweaked freely.
// ================================================================

// -- Window -------------------------------------------------------
constexpr int   WIN_W = 800;
constexpr int   WIN_H = 600;

// -- Paddle ---------------------------------------------------------
constexpr float PADDLE_H = 14.f;
constexpr float PADDLE_SPD = 520.f;
constexpr int   PADDLE_MAX_HP_BASE = 5;     // starting max HP before any shop purchases
constexpr float PADDLE_W_PER_HP = 22.f;  // paddle px of WIDTH per current HP point

// -- Ball -----------------------------------------------------------
constexpr float BALL_RADIUS_BASE = 9.f;
constexpr float BALL_SPD_BASE = 360.f; // px/sec at stage 1, before any modifiers

// -- Bricks -----------------------------------------------------------
constexpr int   BRICK_COLS = 10;
constexpr int   BRICK_ROWS = 5;
constexpr float BRICK_W = 68.f;
constexpr float BRICK_H = 22.f;
constexpr float BRICK_PAD = 6.f;
constexpr float BRICK_OFF_X = 26.f;
constexpr float BRICK_OFF_Y = 60.f;

// Points awarded per brick TYPE/colour (see BrickType enum below). EDIT FREELY.
// Order: RED, ORANGE, GREEN, BLUE, PURPLE
constexpr int BRICK_POINTS[BRICK_ROWS] = { 50, 40, 30, 20, 10 };

// -- Per-stage brick layout: how many rows + which colours can appear -----
// "rows" controls the AMOUNT of bricks (rows * BRICK_COLS), and "types"
// restricts which colours (and therefore which bullet behaviours) can be
// randomly rolled for that stage. Configure as many stages here as you like;
// any stage beyond this table reuses the LAST entry (so once you stop adding
// rows, simply repeat your hardest/final layout -- handy for boss stages
// later: just append more entries with whatever rows/types you want).
//
// BrickType values: RED, ORANGE, GREEN, BLUE, PURPLE  (see enum below)
struct StageBrickDef {
    int       rows;          // 1..BRICK_ROWS bricks-rows used this stage
    int       typeCount;     // how many entries in "types" are valid
    BrickType types[5];      // the allowed colour palette for this stage
};

constexpr StageBrickDef STAGE_BRICK_DEFS[] = {
    // Stage 1: tiny & easy -- only the simplest bullets (slow/thick + baseline)
    { 2, 2, { BrickType::RED, BrickType::PURPLE } },
    // Stage 2: a little bigger, introduce fast bullets
    { 2, 3, { BrickType::RED, BrickType::PURPLE, BrickType::BLUE } },
    // Stage 3: introduce the V-spread orange brick
    { 3, 4, { BrickType::RED, BrickType::ORANGE, BrickType::PURPLE, BrickType::BLUE } },
    // Stage 4: full colour roster unlocked
    { 3, 5, { BrickType::RED, BrickType::ORANGE, BrickType::GREEN, BrickType::BLUE, BrickType::PURPLE } },
    // Stage 5: bigger board, full roster
    { 4, 5, { BrickType::RED, BrickType::ORANGE, BrickType::GREEN, BrickType::BLUE, BrickType::PURPLE } },
    // Stage 6: full board, full roster
    { 5, 5, { BrickType::RED, BrickType::ORANGE, BrickType::GREEN, BrickType::BLUE, BrickType::PURPLE } },
    // Stage 7: BOSS STAGE -- 4 rows instead of the full 5 (one fewer layer)
    // so there's still clear room below for the boss's shield + body.
    // (See BOSS_STAGE_NUMBERS further down -- add more rows like this one
    // for future boss stages, then append a normal full-board entry after
    // if you want stages past it to go back to a regular full board.)
    { 4, 5, { BrickType::RED, BrickType::ORANGE, BrickType::GREEN, BrickType::BLUE, BrickType::PURPLE } },
};
constexpr int NUM_STAGE_BRICK_DEFS = sizeof(STAGE_BRICK_DEFS) / sizeof(STAGE_BRICK_DEFS[0]);

// -- Boss Stages ------------------------------------------------------------
// Which stage numbers (1-based, matching the "STAGE N" HUD label) are boss
// fights. Add more numbers to this list for future bosses (and add a
// matching low-row entry to STAGE_BRICK_DEFS above for each one).
constexpr int BOSS_STAGE_NUMBERS[] = { 7 };
constexpr int NUM_BOSS_STAGE_NUMBERS = sizeof(BOSS_STAGE_NUMBERS) / sizeof(BOSS_STAGE_NUMBERS[0]);

// Boss layout, TOP DOWN: boss body sits near the top of the window (above
// everything, in all 3 phases), then its HP/phase label, then the
// indestructible shield platform, then the bricks underneath that.
constexpr int   BOSS_MAX_HP = 3;
constexpr float BOSS_WIDTH = 480.f;  // boss body width
constexpr float BOSS_HEIGHT = 46.f;   // boss body height
constexpr float BOSS_TOP_MARGIN = 18.f;   // boss body top edge, from window top
constexpr float BOSS_LABEL_GAP = 10.f;   // gap: boss body bottom -> HP/phase label
constexpr float BOSS_LABEL_BLOCK_H = 24.f;   // vertical space for HP pips + phase text
constexpr float BOSS_LABEL_TO_SHIELD_GAP = 8.f;    // gap: label block -> shield platform
constexpr float BOSS_SHIELD_TO_BRICKS_GAP = 16.f;   // gap: shield platform -> bricks below it

// The shield is the boss's only protection -- intentionally small and slow
// so it leaves real gaps for the ball to slip through to the bricks/boss.
constexpr float BOSS_SHIELD_WIDTH = 118.f;  // smaller than a typical paddle
constexpr float BOSS_SHIELD_HEIGHT = 14.f;
constexpr float BOSS_SHIELD_SPEED = 130.f;  // px/sec, slower patrol
constexpr float BOSS_SHIELD_SPAWN_TIME = 0.5f;   // "materialize" animation when it (re)appears

constexpr float BOSS_FLASH_DURATION = 0.35f;  // white flash duration on a successful hit
constexpr int   BOSS_DEFEAT_BONUS_GOLD = 100;     // extra gold on top of the normal stage reward

// Phase 2 (triggers once boss HP drops to 2 of 3): up to this many currently
// -alive bricks gain a shield requiring extra hits before they break.
constexpr int   BOSS_PHASE2_SHIELDED_BRICK_COUNT = 10;
constexpr int   BOSS_PHASE2_EXTRA_HITS_NEEDED = 1;  // +1 hit on top of normal (so 2 total)
constexpr float BOSS_PHASE2_SHIELD_ANIM_TIME = 0.6f; // "shield applied" pulse duration

// Phase 3 (triggers once boss HP drops to 1 of 3): the current top brick row
// fuses into one large brick requiring this many hits to destroy.
constexpr int   BOSS_PHASE3_MEGA_BRICK_HP = 20;
constexpr int   BOSS_MEGA_BRICK_BONUS_POINTS = 300;  // extra score on top of normal brick points
constexpr float BOSS_PHASE3_MERGE_ANIM_TIME = 0.5f; // mega-brick "grow in" animation duration

// -- Pill projectile (base size) ----------------------------------------
// Base size bumped up across the board so bullets are easier to see/dodge.
constexpr float BULLET_W = 10.f;
constexpr float BULLET_H = 26.f;
constexpr float BULLET_SPD_BASE = 195.f; // px/sec at stage 1 (baseline, before per-type mult)

// -- Per-brick-colour bullet behaviour -----------------------------------
// Each brick colour fires a differently-styled projectile. ALL ADJUSTABLE.

// RED -> thick, slow-moving bullet
constexpr float RED_BULLET_W_MULT = 2.0f;   // extra-wide ("thick") pill
constexpr float RED_BULLET_SPD_MULT = 0.55f;  // slower than baseline

// ORANGE -> fires TWO bullets in a downward "V" spread
constexpr float ORANGE_SPREAD_DEG = 45.f; // angle of each arm from straight-down
constexpr float ORANGE_BULLET_SPD_MULT = 1.0f;

// GREEN -> single bullet aimed at the paddle's position at the moment of
// impact (straight-line aim only -- does NOT home/track afterward)
constexpr float GREEN_BULLET_SPD_MULT = 1.0f;

// BLUE -> fast-moving bullet
constexpr float BLUE_BULLET_SPD_MULT = 1.8f;

// PURPLE -> unchanged baseline speed
constexpr float PURPLE_BULLET_SPD_MULT = 1.0f;

// -- Heal-on-combo mechanic --------------------------------------------
// Hit the ball this many times in a row (no bullet damage taken) to heal 1 HP.
constexpr int   HEAL_HITS_NEEDED = 5;

// -- Stage difficulty scaling -------------------------------------------
// NOTE: stage/level number is driven ENTIRELY by clearing all bricks (see
// goToNextStage()) -- it has no relationship to score/points whatsoever.
// Each stage clear increases enemy speed by this % (compounding additively).
constexpr float STAGE_BALL_SPD_GROWTH = 0.12f;  // +12% ball speed / stage
constexpr float STAGE_BULLET_SPD_GROWTH = 0.10f;  // +10% bullet speed / stage

// -- Economy: gold & interest --------------------------------------------
// Gold reward for clearing each of the first 5 stages is set MANUALLY here
// (index 0 = stage 1, ... index 4 = stage 5). Edit these 5 numbers freely.
constexpr int   GOLD_REWARD_MANUAL[5] = { 40, 60, 85, 115, 150 };
// From stage 6 onward, reward grows incrementally past the stage-5 value.
constexpr int   GOLD_PER_STAGE_GROWTH = 30;   // + this much per stage beyond stage 5
constexpr float GOLD_INTEREST_PER_10 = 1.0f; // gold earned per 10 gold SAVED (not spent)
// e.g. 1.0 = 10% interest on banked gold

// Interest is capped: only gold up to this threshold earns interest.
// e.g. base 50 -> floor(50/10)*GOLD_INTEREST_PER_10 = 5 max interest (10%).
// The "Bank Capacity" powerup raises this threshold (see below).
constexpr int   GOLD_INTEREST_CAP_BASE = 50;
constexpr int   GOLD_INTEREST_CAP_GROWTH_PER_LV = 10;  // +10 threshold per purchase

// -- Shop / Powerups ------------------------------------------------------
// Each powerup can be purchased multiple times. Price increases by "growth"
// each time it's bought. Effects stack up to a cap. ALL VALUES BELOW ARE
// FREELY ADJUSTABLE. Every stage-clear, the shop randomly offers 3 of the
// 6 powerups below.

// 1) Vitality: +1 Max Health
constexpr int   PRICE_MAXHP_BASE = 10;
constexpr int   PRICE_MAXHP_GROWTH = 10;
constexpr int   MAXHP_BONUS_CAP = 7;    // can buy up to +7 max HP this run

// 2) Slow-Mo Ball (reduces ball speed)
constexpr int   PRICE_SLOWBALL_BASE = 5;
constexpr int   PRICE_SLOWBALL_GROWTH = 7;
constexpr int   SLOWBALL_LEVEL_CAP = 15;
constexpr float SLOWBALL_REDUCTION_PER_LV = 0.07f; // -7% speed mult per level
constexpr float SLOWBALL_MIN_MULT = 0.35f;     // never goes below 55% speed

// 3) Big Ball (increases ball radius -- easier to hit bricks/return shots)
constexpr int   PRICE_BIGBALL_BASE = 15;
constexpr int   PRICE_BIGBALL_GROWTH = 5;
constexpr int   BIGBALL_LEVEL_CAP = 5;
constexpr float BIGBALL_RADIUS_PER_LV = 1.6f;  // +1.6px radius per level
constexpr float BIGBALL_RADIUS_CAP = 20.f;  // absolute max radius

// 4) Bank Capacity (raises the gold-saved threshold for max interest)
constexpr int   PRICE_INTERESTCAP_BASE = 20;
constexpr int   PRICE_INTERESTCAP_GROWTH = 20;
constexpr int   INTERESTCAP_LEVEL_CAP = 5;   // 50 -> 100 across 5 purchases

// 5) Lucky Brick (chance per brick broken to spawn a bonus ball)
constexpr float LUCKY_PERCENT_PER_LV = 1.0f;   // +1% chance per level
constexpr int   LUCKY_LEVEL_CAP = 20;     // up to 10% chance
constexpr int   PRICE_LUCKY_BASE = 35;
constexpr int   PRICE_LUCKY_GROWTH = 10;

// 6) Bullet Clear (consumable: press E to wipe all bullets on screen)
constexpr int   BULLETCLEAR_BASE_CHARGES = 1;  // every NEW RUN starts with this many
constexpr int   BULLETCLEAR_LEVEL_CAP = 2;  // up to 2 purchases -> 3 total charges/run
constexpr int   PRICE_BULLETCLEAR_BASE = 50;
constexpr int   PRICE_BULLETCLEAR_GROWTH = 70;

// ================================================================
//  COLOURS
// ================================================================
const sf::Color C_BG{ 10,  10,  24 };
const sf::Color C_PADDLE{ 80, 190, 255 };
const sf::Color C_PADDLE_LO{ 220,  55,  55 };
const sf::Color C_PADDLE_MD{ 255, 160,  55 };
const sf::Color C_BALL{ 255, 242, 100 };
const sf::Color C_BALL_BONUS{ 150, 255, 190 };  // bonus balls from Lucky Brick
const sf::Color C_BALL_GLW{ 255, 195,  45 };
const sf::Color C_HEAL{ 100, 255, 145 };
const sf::Color C_GOLD{ 255, 210,  70 };
const sf::Color C_STAR{ 195, 205, 255 };
const sf::Color C_OVERLAY{ 0,   0,   0, 178 };
const sf::Color C_PANEL{ 255, 255, 255,  10 };
const sf::Color C_PANEL_OUT{ 255, 255, 255,  26 };

// Brick colours -- index matches BrickType enum (RED, ORANGE, GREEN, BLUE, PURPLE)
const sf::Color BRICK_COL[BRICK_ROWS] = {
    { 232,  52,  78 },   // RED     50 pts -- thick, slow bullet
    { 232, 138,  32 },   // ORANGE  40 pts -- two bullets in a V spread
    { 172, 228,  32 },   // GREEN   30 pts -- aimed shot at paddle
    {  32, 178, 228 },   // BLUE    20 pts -- fast bullet
    { 148,  52, 228 },   // PURPLE  10 pts -- baseline speed bullet
};

// Bullet colours -- match the firing brick's colour for clarity
const sf::Color C_BULLET_RED{ 255,  90,  90 };
const sf::Color C_BULLET_ORANGE{ 255, 170,  60 };
const sf::Color C_BULLET_GREEN{ 130, 255, 150 };
const sf::Color C_BULLET_BLUE{ 90, 180, 255 };
const sf::Color C_BULLET_PURPLE{ 205, 120, 255 };

// ================================================================
//  TYPES
// ================================================================
enum class State { START, SETTINGS, PLAYING, PAUSED, STAGE_CLEARED, SHOP, GAME_OVER };

// (BrickType enum is declared near the top of the file, before the
// per-stage brick config table, since that table references it.)

struct Brick {
    sf::RectangleShape shape;
    BrickType type = BrickType::RED;
    bool  alive = true;
    int   hp = 1;          // hits needed to destroy (1 = normal; >1 = boss-phase shielded/mega)
    float hitFlash = 0.f;  // brief white flash on a non-fatal (shield-absorbed) hit
    bool  isBossMega = false; // true for the phase-3 fused mega-brick (awards bonus points)
    float shieldApplyAnim = 0.f; // counts down: cyan "shield just applied" pulse (phase 2)
    float spawnAnim = 0.f;       // counts down from 1 -> 0: "growing in" scale anim (phase 3 merge)
};
struct Bullet { sf::ConvexShape    shape; sf::Vector2f vel; bool alive = true; };
struct BallObj { sf::CircleShape    shape; sf::Vector2f vel; bool dead = false; };
struct Star { sf::Vector2f pos; float radius = 1.f; float alpha = 150.f; };

// Boss fight state (see BOSS_STAGE_NUMBERS above to configure which stages are bosses)
struct Boss {
    bool  active = false;
    bool  defeated = false;
    int   hp = BOSS_MAX_HP;
    int   phase = 1;          // 1, 2, or 3
    float shieldY = 0.f;        // y of the indestructible moving platform
    float bodyCenterY = 0.f;        // y-center of the boss body (hittable, ABOVE everything else)
    float shieldX = 0.f;        // current x-center of the shield (patrols back and forth)
    float shieldDir = 1.f;        // +1 or -1
    float flashTimer = 0.f;        // white hit-flash
    float shieldSpawnTimer = 0.f;   // counts down from BOSS_SHIELD_SPAWN_TIME: shield "materialize" anim
};

struct Powerup {
    std::string name;
    std::string desc;
    sf::Color   accent{ 255,255,255 };
    int   level = 0;
    int   levelCap = 0;
    int   priceBase = 0;
    int   priceGrowth = 0;
    int   price() const { return priceBase + level * priceGrowth; }
    bool  maxed() const { return level >= levelCap; }
};

// ================================================================
//  STANDALONE HELPERS
// ================================================================
static bool loadFont(sf::Font& f) {
    const char* P[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    };
    for (auto p : P) if (f.openFromFile(p)) return true;
    return false;
}

static void centreOn(sf::Text& t, float x, float y) {
    sf::FloatRect r = t.getLocalBounds();
    t.setOrigin({ r.position.x + r.size.x * .5f, r.position.y + r.size.y * .5f });
    t.setPosition({ x, y });
}

static sf::Color lighten(sf::Color c, int amt = 28) {
    auto cl = [](int v) { return (uint8_t)std::min(255, v); };
    return sf::Color(cl(c.r + amt), cl(c.g + amt), cl(c.b + amt), c.a);
}

// Vertical capsule / pill shape - local origin at centre.
// Default (unrotated) orientation: long axis runs straight UP/DOWN.
static sf::ConvexShape makePill(float w, float h) {
    constexpr int S = 10;
    sf::ConvexShape s(S * 2);
    float r = w * .5f;
    for (int i = 0; i < S; ++i) {                 // top cap
        float t = PI * (1.f - (float)i / (S - 1));
        s.setPoint(i, { r + r * std::cos(t), r - r * std::sin(t) });
    }
    for (int i = 0; i < S; ++i) {                 // bottom cap
        float t = PI * (float)i / (S - 1);
        s.setPoint(S + i, { r + r * std::cos(t), (h - r) + r * std::sin(t) });
    }
    s.setOrigin({ w * .5f, h * .5f });
    return s;
}

// Random float in [0,100)
static float rollPercent() { return (float)std::rand() / (float)RAND_MAX * 100.f; }

// Is the given 1-based stage number (matches the "STAGE N" HUD label) a boss stage?
static bool isBossStageNum(int stageOneBased) {
    for (int n : BOSS_STAGE_NUMBERS) if (n == stageOneBased) return true;
    return false;
}

// ================================================================
//  GAME
// ================================================================
class Game {
public:
    Game() : window(sf::VideoMode({ (unsigned)WIN_W, (unsigned)WIN_H }), "BrickBreaker") {
        window.setFramerateLimit(60);
        std::srand((unsigned)std::time(nullptr));
        loadFont(font);
        genStars();
        hardResetRun();
        state = State::START;
    }

    void run() {
        sf::Clock clk;
        while (window.isOpen()) {
            float dt = std::min(clk.restart().asSeconds(), .05f);
            handleEvents();
            update(dt);
            render();
        }
    }

private:
    sf::RenderWindow window;
    sf::Font         font;
    State            state = State::START;
    State            settingsReturnState = State::START;

    // -- Settings -------------------------------------------------------
    float masterVolume = 80.f;     // 0-100. Wired for future SFML Audio use.
    bool  draggingVolume = false;

    // -- Paddle -----------------------------------------------------------
    float paddleX = WIN_W * .5f;
    int   paddleHp = PADDLE_MAX_HP_BASE;
    int   currentMaxHp() const { return PADDLE_MAX_HP_BASE + powerups[0].level; }
    float paddleW()   const { return PADDLE_W_PER_HP * (float)paddleHp; }
    float paddleTop() const { return WIN_H - 50.f - PADDLE_H * .5f; }

    // -- Balls (vector supports the Lucky Brick multiball powerup) ----------
    std::vector<BallObj> balls;
    bool ballLaunched = false; // true once the (single, pre-launch) ball has been hit with SPACE

    float ballRadius()    const {
        return std::min(BIGBALL_RADIUS_CAP, BALL_RADIUS_BASE + powerups[2].level * BIGBALL_RADIUS_PER_LV);
    }
    float ballSlowMult()  const {
        return std::max(SLOWBALL_MIN_MULT, 1.f - powerups[1].level * SLOWBALL_REDUCTION_PER_LV);
    }
    void  applyBallRadiusAll() {
        float r = ballRadius();
        for (auto& b : balls) { b.shape.setRadius(r); b.shape.setOrigin({ r, r }); }
    }

    // -- Heal combo -------------------------------------------------------
    int   comboHits = 0;
    float healFlash = 0.f;

    // -- Bullet Clear consumable -------------------------------------------
    int   bulletClearCharges = BULLETCLEAR_BASE_CHARGES;
    float clearFlashTimer = 0.f;

    // -- Game objects -------------------------------------------------------
    std::vector<Brick>  bricks;
    std::vector<Bullet> bullets;
    Boss boss;
    float bossBricksTopY = BRICK_OFF_Y; // where brick row 0 starts; pushed down on boss stages

    // -- Score / stage / economy --------------------------------------------
    int   score = 0;
    int   hiScore = 0;
    int   stageNum = 0;     // 0-based; displayed as stageNum+1
    int   gold = 0;
    int   lastReward = 0;
    int   lastInterest = 0;
    bool  lastStageWasBoss = false; // drives the Stage Cleared screen's title

    float stageBallMult()   const { return 1.f + stageNum * STAGE_BALL_SPD_GROWTH; }
    float stageBulletMult() const { return 1.f + stageNum * STAGE_BULLET_SPD_GROWTH; }
    int   interestCap()     const { return GOLD_INTEREST_CAP_BASE + powerups[3].level * GOLD_INTEREST_CAP_GROWTH_PER_LV; }
    float luckyChance()     const { return powerups[4].level * LUCKY_PERCENT_PER_LV; }

    // -- Shop powerups (persist for the run, reset on hardResetRun) ----------
    // Index/kind reference: 0=Vitality 1=Slow-Mo Ball 2=Big Ball
    //                       3=Bank Capacity 4=Lucky Brick 5=Bullet Clear
    std::array<Powerup, 6> powerups = { {
        { "Vitality",      "+1 Max HP per level, fully heals on purchase",
          sf::Color(255,120,140), 0, MAXHP_BONUS_CAP,       PRICE_MAXHP_BASE,       PRICE_MAXHP_GROWTH },
        { "Slow-Mo Ball",  "Reduces ball speed -- easier to react & block",
          sf::Color(120,200,255), 0, SLOWBALL_LEVEL_CAP,    PRICE_SLOWBALL_BASE,    PRICE_SLOWBALL_GROWTH },
        { "Big Ball",      "Increases ball size -- easier to hit bricks",
          sf::Color(180,140,255), 0, BIGBALL_LEVEL_CAP,     PRICE_BIGBALL_BASE,     PRICE_BIGBALL_GROWTH },
        { "Bank Capacity", "Raises the gold-saved threshold for max interest by 10",
          sf::Color(255,210,90),  0, INTERESTCAP_LEVEL_CAP, PRICE_INTERESTCAP_BASE, PRICE_INTERESTCAP_GROWTH },
        { "Lucky Brick",   "+1% chance per level: breaking a brick spawns a bonus ball",
          sf::Color(150,255,150), 0, LUCKY_LEVEL_CAP,       PRICE_LUCKY_BASE,       PRICE_LUCKY_GROWTH },
        { "Bullet Clear",  "+1 use of [E] Clear-All-Bullets this run (starts at 1, up to 3)",
          sf::Color(255,160,90),  0, BULLETCLEAR_LEVEL_CAP, PRICE_BULLETCLEAR_BASE, PRICE_BULLETCLEAR_GROWTH },
    } };

    std::array<int, 3> shopOffer{ 0,1,2 }; // which 3 powerup indices the shop is currently offering

    // -- Stars ---------------------------------------------------------
    std::vector<Star> stars;

    // =================================================================
    void genStars() {
        stars.clear();
        for (int i = 0; i < 160; ++i) {
            Star s;
            s.pos = { (float)(std::rand() % WIN_W), (float)(std::rand() % WIN_H) };
            s.radius = .5f + (float)(std::rand() % 22) * .1f;
            s.alpha = (float)(55 + std::rand() % 165);
            stars.push_back(s);
        }
    }

    // Full run reset: gold, powerups, score, stage -> back to stage 1
    void hardResetRun() {
        score = 0; gold = 0; stageNum = 0;
        for (auto& p : powerups) p.level = 0;
        bulletClearCharges = BULLETCLEAR_BASE_CHARGES;
        clearFlashTimer = 0.f;
        startStage(true);
    }

    // A fresh ball, resting on the paddle, ready to be launched with SPACE.
    BallObj makeRestingBall() const {
        BallObj b0;
        float r = ballRadius();
        b0.shape.setRadius(r);
        b0.shape.setOrigin({ r, r });
        b0.shape.setFillColor(C_BALL);
        b0.shape.setPosition({ paddleX, (float)WIN_H - 70.f });
        return b0;
    }

    // Begin a stage: rebuild bricks, fully heal/restore paddle & ball(s),
    // and activate/deactivate the boss depending on whether this stage
    // number is listed in BOSS_STAGE_NUMBERS. setupBoss() runs FIRST since
    // it decides where the bricks should start (bricks sit below the boss
    // on boss stages, leaving the boss visible above them).
    void startStage(bool fullHeal) {
        paddleX = WIN_W * .5f;
        if (fullHeal) paddleHp = currentMaxHp();
        ballLaunched = false;
        comboHits = 0; healFlash = 0.f;

        balls.clear();
        balls.push_back(makeRestingBall());

        bullets.clear();
        setupBoss();
        buildBricks();
    }

    // Randomly-generated level: row count AND allowed colour palette come
    // from STAGE_BRICK_DEFS[stageNum] (clamped to the last entry for any
    // stage beyond the table), so early stages can be small/simple and
    // later stages bigger/more varied -- fully configurable up top. On
    // boss stages, bricks start lower (bossBricksTopY) to leave the boss
    // visible above them in every phase.
    void buildBricks() {
        bricks.clear();
        int defIdx = std::min(stageNum, NUM_STAGE_BRICK_DEFS - 1);
        const StageBrickDef& def = STAGE_BRICK_DEFS[defIdx];
        float topY = boss.active ? bossBricksTopY : BRICK_OFF_Y;

        for (int r = 0; r < def.rows; ++r)
            for (int c = 0; c < BRICK_COLS; ++c) {
                Brick b;
                b.type = def.types[std::rand() % def.typeCount];
                int ti = (int)b.type;
                b.shape.setSize({ BRICK_W, BRICK_H });
                b.shape.setPosition({
                    BRICK_OFF_X + c * (BRICK_W + BRICK_PAD),
                    topY + r * (BRICK_H + BRICK_PAD)
                    });
                b.shape.setFillColor(BRICK_COL[ti]);
                b.shape.setOutlineThickness(1.f);
                b.shape.setOutlineColor({ 0, 0, 0, 50 });
                bricks.push_back(std::move(b));
            }
    }

    // Activates the boss (positioned near the TOP of the window, above
    // everything -- shield platform and bricks both sit below it, in every
    // phase) if the upcoming stage number is listed in BOSS_STAGE_NUMBERS;
    // otherwise leaves the boss fully inactive.
    void setupBoss() {
        boss = Boss{};
        bossBricksTopY = BRICK_OFF_Y;
        if (!isBossStageNum(stageNum + 1)) return;

        boss.active = true;
        float bodyTop = BOSS_TOP_MARGIN;
        boss.bodyCenterY = bodyTop + BOSS_HEIGHT * .5f;
        float labelTop = bodyTop + BOSS_HEIGHT + BOSS_LABEL_GAP;
        boss.shieldY = labelTop + BOSS_LABEL_BLOCK_H + BOSS_LABEL_TO_SHIELD_GAP;
        boss.shieldX = WIN_W * .5f;
        boss.shieldDir = 1.f;
        boss.shieldSpawnTimer = BOSS_SHIELD_SPAWN_TIME;

        bossBricksTopY = boss.shieldY + BOSS_SHIELD_HEIGHT + BOSS_SHIELD_TO_BRICKS_GAP;
    }

    sf::FloatRect bossBodyRect() const {
        return { { WIN_W * .5f - BOSS_WIDTH * .5f, boss.bodyCenterY - BOSS_HEIGHT * .5f }, { BOSS_WIDTH, BOSS_HEIGHT } };
    }
    sf::FloatRect bossShieldRect() const {
        return { { boss.shieldX - BOSS_SHIELD_WIDTH * .5f, boss.shieldY }, { BOSS_SHIELD_WIDTH, BOSS_SHIELD_HEIGHT } };
    }

    // Marks the boss defeated, awards its bonus gold on top of the normal
    // stage-clear reward, and routes to the Stage Cleared screen.
    void defeatBoss() {
        boss.defeated = true;
        boss.active = false;
        lastStageWasBoss = true;
        triggerStageCleared();
        lastReward += BOSS_DEFEAT_BONUS_GOLD;
        gold += BOSS_DEFEAT_BONUS_GOLD;
    }

    // TEMPORARY GOD-MODE CHEAT -- set to false (or delete the F1 handling
    // in handleKey()) to remove before a real release build. On a boss
    // stage this instantly defeats the boss; otherwise it clears all bricks.
    static constexpr bool GOD_MODE_CHEAT_ENABLED = true;
    void cheatClearAllBricks() {
        if (boss.active && !boss.defeated) {
            defeatBoss();
            return;
        }
        for (auto& b : bricks) b.alive = false;
        lastStageWasBoss = false;
        triggerStageCleared();
    }


    // =================================================================
    //  BUTTON / UI RECT LAYOUT  (shared by render + click handling)
    // =================================================================
    sf::FloatRect playBtnRect()     const { return { { WIN_W * .5f - 120.f, 296.f }, { 240.f, 56.f } }; }
    sf::FloatRect settingsBtnRect() const { return { { WIN_W * .5f - 120.f, 366.f }, { 240.f, 56.f } }; }
    sf::FloatRect exitBtnRect()     const { return { { WIN_W * .5f - 120.f, 436.f }, { 240.f, 56.f } }; }

    sf::FloatRect settingsBackRect() const { return { { WIN_W * .5f - 100.f, 522.f }, { 200.f, 50.f } }; }
    sf::FloatRect volumeSliderRect() const { return { { WIN_W * .5f - 150.f, 330.f }, { 300.f, 10.f } }; }

    sf::FloatRect stageContinueRect() const { return { { WIN_W * .5f - 130.f, 490.f }, { 260.f, 56.f } }; }

    sf::FloatRect shopBuyRect(int idx) const {
        float y = 222.f + idx * 100.f;
        return { { WIN_W * .5f + 70.f, y }, { 150.f, 46.f } };
    }
    sf::FloatRect shopNextRect() const { return { { WIN_W * .5f - 130.f, 540.f }, { 260.f, 48.f } }; }

    sf::FloatRect pauseResumeRect()   const { return { { WIN_W * .5f - 110.f, 280.f }, { 220.f, 50.f } }; }
    sf::FloatRect pauseRestartRect()  const { return { { WIN_W * .5f - 110.f, 340.f }, { 220.f, 50.f } }; }
    sf::FloatRect pauseMenuRect()     const { return { { WIN_W * .5f - 110.f, 400.f }, { 220.f, 50.f } }; }

    sf::FloatRect goPlayAgainRect()   const { return { { WIN_W * .5f - 130.f, 340.f }, { 260.f, 52.f } }; }
    sf::FloatRect goMenuRect()        const { return { { WIN_W * .5f - 130.f, 404.f }, { 260.f, 52.f } }; }

    static bool hit(const sf::FloatRect& r, sf::Vector2f p) { return r.contains(p); }

    // =================================================================
    //  EVENTS
    // =================================================================
    void handleEvents() {
        while (auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) { window.close(); return; }

            if (const auto* kp = ev->getIf<sf::Event::KeyPressed>())
                handleKey(kp->code);

            if (const auto* mb = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mp{ (float)mb->position.x, (float)mb->position.y };
                    handleClick(mp);
                }
            }
            if (const auto* mu = ev->getIf<sf::Event::MouseButtonReleased>()) {
                if (mu->button == sf::Mouse::Button::Left) draggingVolume = false;
            }
            if (const auto* mm = ev->getIf<sf::Event::MouseMoved>()) {
                if (draggingVolume && state == State::SETTINGS)
                    updateVolumeFromMouseX((float)mm->position.x);
            }
        }
    }

    void handleKey(sf::Keyboard::Key k) {
        using K = sf::Keyboard::Key;
        switch (state) {
        case State::START:
            if (k == K::Enter || k == K::Space) { hardResetRun(); state = State::PLAYING; }
            break;
        case State::SETTINGS:
            if (k == K::Escape) state = settingsReturnState;
            break;
        case State::PLAYING:
            if (k == K::Escape || k == K::P) state = State::PAUSED;
            if (!ballLaunched && (k == K::Space || k == K::Up)) launchBall();
            if (k == K::E) tryClearBullets();
            // TEMPORARY GOD-MODE CHEAT (testing only) -- instantly clears
            // every brick on screen, triggering the normal stage-cleared flow.
            if (GOD_MODE_CHEAT_ENABLED && k == K::F1 &&
                (!bricks.empty() || (boss.active && !boss.defeated)))
                cheatClearAllBricks();
            break;
        case State::PAUSED:
            if (k == K::Escape || k == K::P) state = State::PLAYING;
            if (k == K::R) { hardResetRun(); state = State::PLAYING; }
            if (k == K::M) state = State::START;
            break;
        case State::STAGE_CLEARED:
            if (k == K::Space || k == K::Enter) enterShop();
            break;
        case State::SHOP:
            if (k == K::Space || k == K::Enter) goToNextStage();
            break;
        case State::GAME_OVER:
            if (k == K::Enter || k == K::Space) { hardResetRun(); state = State::PLAYING; }
            if (k == K::M) state = State::START;
            break;
        }
    }

    void handleClick(sf::Vector2f mp) {
        switch (state) {
        case State::START:
            if (hit(playBtnRect(), mp)) { hardResetRun(); state = State::PLAYING; }
            else if (hit(settingsBtnRect(), mp)) { settingsReturnState = State::START; state = State::SETTINGS; }
            else if (hit(exitBtnRect(), mp)) { window.close(); }
            break;

        case State::SETTINGS:
            if (hit(settingsBackRect(), mp)) state = settingsReturnState;
            else if (hit(volumeSliderRect(), mp)) { draggingVolume = true; updateVolumeFromMouseX(mp.x); }
            break;

        case State::PAUSED:
            if (hit(pauseResumeRect(), mp))  state = State::PLAYING;
            else if (hit(pauseRestartRect(), mp)) { hardResetRun(); state = State::PLAYING; }
            else if (hit(pauseMenuRect(), mp))    state = State::START;
            break;

        case State::STAGE_CLEARED:
            if (hit(stageContinueRect(), mp)) enterShop();
            break;

        case State::SHOP:
            if (hit(shopBuyRect(0), mp)) tryBuy(shopOffer[0]);
            else if (hit(shopBuyRect(1), mp)) tryBuy(shopOffer[1]);
            else if (hit(shopBuyRect(2), mp)) tryBuy(shopOffer[2]);
            else if (hit(shopNextRect(), mp)) goToNextStage();
            break;

        case State::GAME_OVER:
            if (hit(goPlayAgainRect(), mp)) { hardResetRun(); state = State::PLAYING; }
            else if (hit(goMenuRect(), mp)) state = State::START;
            break;

        default: break;
        }
    }

    void updateVolumeFromMouseX(float x) {
        auto r = volumeSliderRect();
        float frac = (x - r.position.x) / r.size.x;
        frac = std::max(0.f, std::min(1.f, frac));
        masterVolume = frac * 100.f;
        // NOTE: to make this control actual sound, add the SFML Audio module
        // (link sfml-audio.lib) and call sf::Listener::setGlobalVolume(masterVolume).
    }

    void tryClearBullets() {
        if (bulletClearCharges <= 0) return;
        bullets.clear();
        --bulletClearCharges;
        clearFlashTimer = 0.6f;
    }

    // kind: 0=Vitality 1=Slow-Mo Ball 2=Big Ball 3=Bank Capacity 4=Lucky Brick 5=Bullet Clear
    void tryBuy(int kind) {
        Powerup& p = powerups[kind];
        if (p.maxed() || gold < p.price()) return;
        gold -= p.price();
        p.level++;
        switch (kind) {
        case 0: paddleHp = currentMaxHp(); break;   // full heal on purchase
        case 1: break;                              // ballSlowMult() reads level live
        case 2: applyBallRadiusAll(); break;
        case 3: break;                              // interestCap() reads level live
        case 4: break;                              // luckyChance() reads level live
        case 5: ++bulletClearCharges; break;         // immediate extra charge
        }
    }

    // Randomly picks 3 of the 6 powerups to offer, preferring ones not yet maxed.
    void rollShopOffers() {
        std::vector<int> notMaxed, maxedOut;
        for (int i = 0; i < (int)powerups.size(); ++i)
            (powerups[i].maxed() ? maxedOut : notMaxed).push_back(i);

        auto shuffle = [](std::vector<int>& v) {
            for (size_t i = v.size(); i > 1; --i)
                std::swap(v[i - 1], v[(size_t)(std::rand() % (int)i)]);
            };
        shuffle(notMaxed);
        shuffle(maxedOut);

        std::vector<int> picks;
        for (int idx : notMaxed) { if (picks.size() >= 3) break; picks.push_back(idx); }
        for (int idx : maxedOut) { if (picks.size() >= 3) break; picks.push_back(idx); }

        for (int i = 0; i < 3; ++i)
            shopOffer[i] = (i < (int)picks.size()) ? picks[i] : picks.back();
    }

    void enterShop() {
        rollShopOffers();
        state = State::SHOP;
    }

    void goToNextStage() {
        ++stageNum;
        startStage(true);
        state = State::PLAYING;
    }

    void launchBall() {
        ballLaunched = true;
        float ang = -70.f + (float)(std::rand() % 40);
        float rad = ang * PI / 180.f;
        float spd = BALL_SPD_BASE * stageBallMult() * ballSlowMult();
        if (!balls.empty())
            balls[0].vel = { spd * std::sin(rad), -std::abs(spd * std::cos(rad)) };
    }

    // =================================================================
    //  UPDATE
    // =================================================================
    void update(float dt) {
        if (state != State::PLAYING) return;

        if (healFlash > 0.f) healFlash -= dt;
        if (clearFlashTimer > 0.f) clearFlashTimer -= dt;
        for (auto& b : bricks) {
            if (b.hitFlash > 0.f) b.hitFlash -= dt;
            if (b.shieldApplyAnim > 0.f) b.shieldApplyAnim -= dt;
            if (b.spawnAnim > 0.f) b.spawnAnim = std::max(0.f, b.spawnAnim - dt);
        }
        updateBoss(dt);
        movePaddle(dt);

        if (!ballLaunched) {
            if (!balls.empty())
                balls[0].shape.setPosition({ paddleX, paddleTop() - ballRadius() - 2.f });
        }
        else {
            moveBalls(dt);
        }

        if (state == State::PLAYING) updateBullets(dt);
    }

    void updateBoss(float dt) {
        if (!boss.active || boss.defeated) return;
        if (boss.flashTimer > 0.f) boss.flashTimer -= dt;
        if (boss.shieldSpawnTimer > 0.f) boss.shieldSpawnTimer = std::max(0.f, boss.shieldSpawnTimer - dt);

        float halfRange = BOSS_WIDTH * .5f - BOSS_SHIELD_WIDTH * .5f;
        float minX = WIN_W * .5f - halfRange, maxX = WIN_W * .5f + halfRange;
        boss.shieldX += boss.shieldDir * BOSS_SHIELD_SPEED * dt;
        if (boss.shieldX < minX) { boss.shieldX = minX; boss.shieldDir = 1.f; }
        if (boss.shieldX > maxX) { boss.shieldX = maxX; boss.shieldDir = -1.f; }
    }


    void movePaddle(float dt) {
        using K = sf::Keyboard::Key;
        if (sf::Keyboard::isKeyPressed(K::Left) || sf::Keyboard::isKeyPressed(K::A))
            paddleX -= PADDLE_SPD * dt;
        if (sf::Keyboard::isKeyPressed(K::Right) || sf::Keyboard::isKeyPressed(K::D))
            paddleX += PADDLE_SPD * dt;
        float hw = paddleW() * .5f;
        paddleX = std::max(hw, std::min((float)WIN_W - hw, paddleX));
    }

    // Moves every active ball, handles wall/boss/paddle/brick collisions for
    // each, removes any that fall off-screen, and spawns Lucky-Brick bonus
    // balls (deferred until after the main loop to avoid iterator invalidation).
    void moveBalls(float dt) {
        float br = ballRadius();
        float hw = paddleW() * .5f;
        float ptop = paddleTop();
        float spd = BALL_SPD_BASE * stageBallMult() * ballSlowMult();

        std::vector<sf::Vector2f> luckySpawns;
        bool bossHitThisFrame = false;

        for (auto& B : balls) {
            sf::Vector2f pos = B.shape.getPosition() + B.vel * dt;

            if (pos.x - br < 0.f) { pos.x = br;            B.vel.x = std::abs(B.vel.x); }
            if (pos.x + br > WIN_W) { pos.x = WIN_W - br;     B.vel.x = -std::abs(B.vel.x); }
            if (pos.y - br < 0.f) { pos.y = br;             B.vel.y = std::abs(B.vel.y); }

            if (pos.y - br > WIN_H) { B.dead = true; continue; }

            // Boss collision: the shield is an indestructible wall (just
            // bounces the ball back up); the boss BODY beneath it is the
            // actual hittable target -- touching it damages the boss and
            // sends every ball back to the player's hand.
            if (boss.active && !boss.defeated) {
                sf::FloatRect ballRect({ pos.x - br, pos.y - br }, { br * 2.f, br * 2.f });
                sf::FloatRect shieldRect = bossShieldRect();
                sf::FloatRect bodyRect = bossBodyRect();

                if (ballRect.findIntersection(shieldRect)) {
                    pos.y = shieldRect.position.y - br;
                    B.vel.y = -std::abs(B.vel.y);
                    B.shape.setPosition(pos);
                    continue;
                }
                if (ballRect.findIntersection(bodyRect)) {
                    bossHitThisFrame = true;
                    B.dead = true;
                    continue;
                }
            }

            // Paddle bounce
            if (B.vel.y > 0.f &&
                pos.x >= paddleX - hw && pos.x <= paddleX + hw &&
                pos.y + br >= ptop && pos.y + br <= ptop + PADDLE_H + 12.f)
            {
                pos.y = ptop - br;
                B.vel.y = -std::abs(B.vel.y);
                float off = (pos.x - paddleX) / hw;
                B.vel.x = off * spd;
                float mag = std::sqrt(B.vel.x * B.vel.x + B.vel.y * B.vel.y);
                if (mag > 0.f) B.vel = B.vel / mag * spd;

                ++comboHits;
                if (comboHits >= HEAL_HITS_NEEDED && paddleHp < currentMaxHp()) {
                    ++paddleHp; comboHits = 0; healFlash = 0.75f;
                }
            }

            // Brick collisions (supports multi-hit bricks: boss-phase shields
            // need BOSS_PHASE2_EXTRA_HITS_NEEDED extra hits, and the phase-3
            // mega-brick needs BOSS_PHASE3_MEGA_BRICK_HP hits)
            sf::FloatRect ballRect({ pos.x - br, pos.y - br }, { br * 2.f, br * 2.f });
            for (auto& b : bricks) {
                if (!b.alive) continue;
                sf::FloatRect bb = b.shape.getGlobalBounds();
                if (ballRect.findIntersection(bb)) {
                    --b.hp;
                    if (b.hp <= 0) {
                        b.alive = false;
                        score += BRICK_POINTS[(int)b.type];
                        if (b.isBossMega) score += BOSS_MEGA_BRICK_BONUS_POINTS;
                        // Bullet(s) spawn ONLY from the brick that was just broken;
                        // behaviour (count/speed/angle/aim) depends on its colour.
                        fireBulletsFromBrick(b.type, bb);

                        // Lucky Brick: chance to spawn a bonus ball on ANY brick break.
                        if (luckyChance() > 0.f && rollPercent() < luckyChance())
                            luckySpawns.push_back({ bb.position.x + bb.size.x * .5f,
                                                    bb.position.y + bb.size.y * .5f });
                    }
                    else {
                        b.hitFlash = 0.18f; // still alive (shield/mega absorbed a hit)
                    }

                    float oL = (ballRect.position.x + ballRect.size.x) - bb.position.x;
                    float oR = (bb.position.x + bb.size.x) - ballRect.position.x;
                    float oT = (ballRect.position.y + ballRect.size.y) - bb.position.y;
                    float oB = (bb.position.y + bb.size.y) - ballRect.position.y;
                    if (std::min(oL, oR) < std::min(oT, oB)) B.vel.x = -B.vel.x;
                    else                                     B.vel.y = -B.vel.y;
                    break;
                }
            }

            B.shape.setPosition(pos);
        }

        balls.erase(std::remove_if(balls.begin(), balls.end(),
            [](const BallObj& b) { return b.dead; }), balls.end());

        if (bossHitThisFrame) {
            registerBossHit();
            if (state != State::PLAYING) return; // boss defeat already moved us to STAGE_CLEARED
            resetBallToHand();
            return;
        }

        if (balls.empty()) {
            hiScore = std::max(hiScore, score);
            state = State::GAME_OVER;
            return;
        }

        for (auto& sp : luckySpawns) spawnExtraBall(sp.x, sp.y);

        if (!boss.active || boss.defeated) {
            bool anyAlive = std::any_of(bricks.begin(), bricks.end(), [](const Brick& b) { return b.alive; });
            if (!anyAlive) { lastStageWasBoss = false; triggerStageCleared(); }
        }
    }

    // Damages the boss, advances its phase (resetting the bricks to a fresh
    // "new level" layout and then applying that phase's mechanic on top),
    // or defeats it outright if this was its last HP.
    void registerBossHit() {
        boss.flashTimer = BOSS_FLASH_DURATION;
        --boss.hp;
        if (boss.hp == 2 && boss.phase == 1) {
            boss.phase = 2;
            buildBricks();        // fresh brick layout, as if starting a new level
            applyPhase2Shields(); // then layer this phase's shield mechanic on top
            boss.shieldSpawnTimer = BOSS_SHIELD_SPAWN_TIME; // shield "re-materializes" too
        }
        else if (boss.hp == 1 && boss.phase == 2) {
            boss.phase = 3;
            buildBricks();          // fresh brick layout, as if starting a new level
            applyPhase3MegaBrick(); // then fuse its top row into the mega-brick
            boss.shieldSpawnTimer = BOSS_SHIELD_SPAWN_TIME;
        }
        else if (boss.hp <= 0) {
            defeatBoss();
        }
    }

    // Phase 2: up to BOSS_PHASE2_SHIELDED_BRICK_COUNT random currently-alive
    // bricks gain extra HP (a "shield") that must be hit through before they
    // break. Each one gets a brief cyan pulse animation to announce it.
    void applyPhase2Shields() {
        std::vector<int> idx;
        for (int i = 0; i < (int)bricks.size(); ++i) if (bricks[i].alive) idx.push_back(i);
        for (size_t i = idx.size(); i > 1; --i)
            std::swap(idx[i - 1], idx[(size_t)(std::rand() % (int)i)]);
        int n = std::min((int)idx.size(), BOSS_PHASE2_SHIELDED_BRICK_COUNT);
        for (int k = 0; k < n; ++k) {
            bricks[idx[k]].hp += BOSS_PHASE2_EXTRA_HITS_NEEDED;
            bricks[idx[k]].shieldApplyAnim = BOSS_PHASE2_SHIELD_ANIM_TIME;
        }
    }

    // Phase 3: fuses the current top-most surviving brick row into one large
    // brick requiring BOSS_PHASE3_MEGA_BRICK_HP hits, with a brief "growing
    // in" scale animation. If no bricks remain (player already cleared
    // them), spawns a fresh mega-brick where the top row would normally
    // sit, so the mechanic always has something to fuse.
    void applyPhase3MegaBrick() {
        float left = BRICK_OFF_X;
        float right = BRICK_OFF_X + BRICK_COLS * (BRICK_W + BRICK_PAD) - BRICK_PAD;
        float topY = bossBricksTopY;

        float minY = 1e9f;
        for (auto& b : bricks) if (b.alive) minY = std::min(minY, b.shape.getPosition().y);
        if (minY < 1e8f) {
            topY = minY;
            left = 1e9f; right = -1e9f;
            for (auto& b : bricks) {
                if (!b.alive || std::abs(b.shape.getPosition().y - minY) > 1.f) continue;
                left = std::min(left, b.shape.getPosition().x);
                right = std::max(right, b.shape.getPosition().x + BRICK_W);
            }
            bricks.erase(std::remove_if(bricks.begin(), bricks.end(), [&](const Brick& b) {
                return b.alive && std::abs(b.shape.getPosition().y - minY) < 1.f;
                }), bricks.end());
        }

        Brick mega;
        mega.type = BrickType::PURPLE; // baseline colour/points; visuals overridden below
        mega.hp = BOSS_PHASE3_MEGA_BRICK_HP;
        mega.isBossMega = true;
        mega.spawnAnim = BOSS_PHASE3_MERGE_ANIM_TIME; // counts down to 0 -> drives the grow-in
        mega.shape.setSize({ right - left, BRICK_H });
        mega.shape.setPosition({ left, topY });
        mega.shape.setFillColor(sf::Color(40, 10, 55));
        mega.shape.setOutlineThickness(2.5f);
        mega.shape.setOutlineColor(sf::Color(255, 80, 180, 200));
        bricks.push_back(std::move(mega));
    }

    // Wipes all balls and gives the player a single fresh resting ball --
    // used when the player's ball connects with the boss (per the boss
    // design: every successful hit costs you your current ball(s)).
    void resetBallToHand() {
        balls.clear();
        balls.push_back(makeRestingBall());
        ballLaunched = false;
        comboHits = 0;
    }


    void spawnExtraBall(float x, float y) {
        BallObj nb;
        float r = ballRadius();
        nb.shape.setRadius(r);
        nb.shape.setOrigin({ r, r });
        nb.shape.setFillColor(C_BALL_BONUS);
        nb.shape.setPosition({ x, y });
        float ang = -70.f + (float)(std::rand() % 40);
        float rad = ang * PI / 180.f;
        float spd = BALL_SPD_BASE * stageBallMult() * ballSlowMult();
        nb.vel = { spd * std::sin(rad), -std::abs(spd * std::cos(rad)) };
        balls.push_back(std::move(nb));
    }

    // Spawns a single pill-shaped bullet with an explicit velocity vector.
    // The pill is rotated to visually align with its travel direction.
    void spawnBulletDir(float x, float y, sf::Vector2f vel, float w, float h, sf::Color col) {
        Bullet bl;
        bl.shape = makePill(w, h);
        bl.shape.setFillColor(col);
        bl.shape.setPosition({ x, y + h * .5f });
        // Pill's un-rotated long axis points straight DOWN (local +Y).
        // SFML rotation is clockwise in this Y-down coordinate system, so to
        // align the axis with vel=(vx,vy) we need angle = -atan2(vx,vy).
        float angDeg = -std::atan2(vel.x, vel.y) * 180.f / PI;
        bl.shape.setRotation(sf::degrees(angDeg));
        bl.vel = vel;
        bullets.push_back(std::move(bl));
    }

    // Dispatches the correct bullet pattern for the brick colour that just broke.
    void fireBulletsFromBrick(BrickType type, sf::FloatRect bb) {
        float cx = bb.position.x + bb.size.x * .5f;
        float cy = bb.position.y + bb.size.y;
        float baseSpd = BULLET_SPD_BASE * stageBulletMult();

        switch (type) {
        case BrickType::RED: {
            // Thick, slow-moving single bullet straight down.
            float spd = baseSpd * RED_BULLET_SPD_MULT;
            spawnBulletDir(cx, cy, { 0.f, spd }, BULLET_W * RED_BULLET_W_MULT, BULLET_H, C_BULLET_RED);
            break;
        }
        case BrickType::ORANGE: {
            // Two bullets diverging outward/downward in a 45-degree "V".
            float spd = baseSpd * ORANGE_BULLET_SPD_MULT;
            float rad = ORANGE_SPREAD_DEG * PI / 180.f;
            sf::Vector2f vL{ -spd * std::sin(rad), spd * std::cos(rad) };
            sf::Vector2f vR{ spd * std::sin(rad), spd * std::cos(rad) };
            spawnBulletDir(cx, cy, vL, BULLET_W, BULLET_H, C_BULLET_ORANGE);
            spawnBulletDir(cx, cy, vR, BULLET_W, BULLET_H, C_BULLET_ORANGE);
            break;
        }
        case BrickType::GREEN: {
            // Single bullet aimed at the paddle's position right now.
            // Pure straight-line aim -- it does NOT track/home afterward.
            float spd = baseSpd * GREEN_BULLET_SPD_MULT;
            sf::Vector2f toPaddle{ paddleX - cx, paddleTop() - cy };
            float len = std::sqrt(toPaddle.x * toPaddle.x + toPaddle.y * toPaddle.y);
            sf::Vector2f dir = (len > 0.001f) ? toPaddle / len : sf::Vector2f{ 0.f, 1.f };
            spawnBulletDir(cx, cy, dir * spd, BULLET_W, BULLET_H, C_BULLET_GREEN);
            break;
        }
        case BrickType::BLUE: {
            // Fast single bullet straight down.
            float spd = baseSpd * BLUE_BULLET_SPD_MULT;
            spawnBulletDir(cx, cy, { 0.f, spd }, BULLET_W, BULLET_H, C_BULLET_BLUE);
            break;
        }
        case BrickType::PURPLE:
        default: {
            // Baseline speed, unchanged from the original single bullet.
            float spd = baseSpd * PURPLE_BULLET_SPD_MULT;
            spawnBulletDir(cx, cy, { 0.f, spd }, BULLET_W, BULLET_H, C_BULLET_PURPLE);
            break;
        }
        }
    }

    void updateBullets(float dt) {
        float hw = paddleW() * .5f;
        sf::FloatRect pRect({ paddleX - hw, paddleTop() }, { paddleW(), PADDLE_H });

        for (auto& bl : bullets) {
            if (!bl.alive) continue;
            bl.shape.move(bl.vel * dt);
            auto p = bl.shape.getPosition();
            // Angled bullets (orange/green) can exit the sides, not just the bottom.
            if (p.y - BULLET_H * .5f > WIN_H + 40.f || p.x < -40.f || p.x > WIN_W + 40.f) {
                bl.alive = false; continue;
            }
            if (bl.shape.getGlobalBounds().findIntersection(pRect)) {
                bl.alive = false; --paddleHp; comboHits = 0;
                if (paddleHp <= 0) {
                    hiScore = std::max(hiScore, score);
                    state = State::GAME_OVER;
                    return;
                }
            }
        }
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return !b.alive; }), bullets.end());
    }

    // Stages 1-5 (stageNum 0-4) use the manually-set GOLD_REWARD_MANUAL table.
    // Stage 6+ grows incrementally past the stage-5 value.
    static int goldRewardForStage(int stageIdx) {
        if (stageIdx < 5) return GOLD_REWARD_MANUAL[stageIdx];
        int beyond = stageIdx - 4; // stage 6 (idx 5) -> beyond = 1
        return GOLD_REWARD_MANUAL[4] + beyond * GOLD_PER_STAGE_GROWTH;
    }

    void triggerStageCleared() {
        hiScore = std::max(hiScore, score);
        int cap = interestCap();
        int effectiveGold = std::min(gold, cap);
        lastInterest = (int)(std::floor(effectiveGold / 10.f) * GOLD_INTEREST_PER_10);
        lastReward = goldRewardForStage(stageNum);
        gold += lastReward + lastInterest;
        ballLaunched = false;
        bullets.clear();
        state = State::STAGE_CLEARED;
    }

    // =================================================================
    //  RENDER
    // =================================================================
    void render() {
        window.clear(C_BG);
        drawStars();
        sf::Vector2f mp = (sf::Vector2f)sf::Mouse::getPosition(window);

        switch (state) {
        case State::START:         drawStart(mp);                                 break;
        case State::SETTINGS:      drawSettings(mp);                              break;
        case State::PLAYING:       drawGame();                                    break;
        case State::PAUSED:        drawGame(); drawPauseOverlay(mp);              break;
        case State::STAGE_CLEARED: drawGame(); drawStageClearedOverlay(mp);       break;
        case State::SHOP:          drawGame(); drawShopOverlay(mp);               break;
        case State::GAME_OVER:     drawGame(); drawGameOverOverlay(mp);           break;
        }
        window.display();
    }

    void drawStars() {
        for (auto& s : stars) {
            sf::CircleShape c(s.radius);
            c.setOrigin({ s.radius, s.radius });
            c.setPosition(s.pos);
            c.setFillColor({ C_STAR.r, C_STAR.g, C_STAR.b, (uint8_t)s.alpha });
            window.draw(c);
        }
    }

    void ballGlow(const BallObj& B) {
        auto pos = B.shape.getPosition();
        float r = B.shape.getRadius();
        sf::Color base = lighten(B.shape.getFillColor(), 10);
        for (int i = 4; i >= 1; --i) {
            float rr = r * (1.f + i * .5f);
            sf::CircleShape c(rr);
            c.setOrigin({ rr, rr }); c.setPosition(pos);
            c.setFillColor({ base.r, base.g, base.b, (uint8_t)(38 / i) });
            window.draw(c);
        }
    }
    void bulletGlow(const Bullet& bl) {
        sf::Color base = lighten(bl.shape.getFillColor(), 35);
        for (int i = 3; i >= 1; --i) {
            sf::ConvexShape g = bl.shape;
            float sc = 1.f + i * .42f;
            g.setScale({ sc, sc });
            g.setFillColor({ base.r, base.g, base.b, (uint8_t)(42 / i) });
            window.draw(g);
        }
    }

    void drawGame() {
        for (auto& b : bricks) {
            if (!b.alive) continue;
            sf::Vector2f sz = b.shape.getSize();
            sf::Vector2f pos = b.shape.getPosition();
            sf::Vector2f center = { pos.x + sz.x * .5f, pos.y + sz.y * .5f };

            // Phase-3 mega-brick "merging" grow-in: scales up horizontally
            // from a thin sliver to full width over BOSS_PHASE3_MERGE_ANIM_TIME.
            float growT = 1.f;
            if (b.spawnAnim > 0.f && BOSS_PHASE3_MERGE_ANIM_TIME > 0.f) {
                float frac = b.spawnAnim / BOSS_PHASE3_MERGE_ANIM_TIME; // 1 -> 0
                growT = std::max(0.05f, 1.f - frac);
            }

            sf::RectangleShape body = b.shape;
            body.setOrigin({ sz.x * .5f, sz.y * .5f });
            body.setPosition(center);
            body.setScale({ growT, 1.f });
            window.draw(body);

            if (b.hitFlash > 0.f) {
                sf::RectangleShape flash(sz);
                flash.setOrigin({ sz.x * .5f, sz.y * .5f });
                flash.setPosition(center);
                flash.setScale({ growT, 1.f });
                flash.setFillColor({ 255,255,255, (uint8_t)(190.f * (b.hitFlash / 0.18f)) });
                window.draw(flash);
            }

            // Shine/shadow strips only once the merge animation has mostly settled
            if (growT > 0.85f) {
                sf::RectangleShape sh({ sz.x - 6.f, 3.f });
                sh.setPosition({ pos.x + 3.f, pos.y + 2.5f });
                sh.setFillColor({ 255,255,255,50 });
                window.draw(sh);
                sf::RectangleShape sd({ sz.x - 6.f, 2.f });
                sd.setPosition({ pos.x + 3.f, pos.y + sz.y - 3.f });
                sd.setFillColor({ 0,0,0,45 });
                window.draw(sd);
            }

            // Shield ring + remaining-hits readout for multi-hit bricks
            if (b.hp > 1) {
                sf::RectangleShape ring(sz);
                ring.setOrigin({ sz.x * .5f, sz.y * .5f });
                ring.setPosition(center);
                ring.setScale({ growT, 1.f });
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineThickness(2.f);
                ring.setOutlineColor(sf::Color(120, 230, 255, 210));
                window.draw(ring);
                if (b.isBossMega && growT > 0.5f) {
                    auto hpT = txt(std::to_string(b.hp), 16, sf::Color::White);
                    centreOn(hpT, center.x, center.y);
                    window.draw(hpT);
                }
            }

            // Phase-2 "shield just applied" announcement: an expanding,
            // fading cyan ring around the brick (distinct from the steady
            // shield ring above, which stays for as long as hp > 1).
            if (b.shieldApplyAnim > 0.f) {
                float t = 1.f - (b.shieldApplyAnim / BOSS_PHASE2_SHIELD_ANIM_TIME); // 0 -> 1
                float pad = t * 10.f;
                sf::RectangleShape pulse({ sz.x + pad * 2.f, sz.y + pad * 2.f });
                pulse.setOrigin({ (sz.x + pad * 2.f) * .5f, (sz.y + pad * 2.f) * .5f });
                pulse.setPosition(center);
                pulse.setFillColor(sf::Color::Transparent);
                pulse.setOutlineThickness(2.f);
                pulse.setOutlineColor(sf::Color(120, 230, 255, (uint8_t)(220.f * (1.f - t))));
                window.draw(pulse);
            }
        }

        drawBoss();

        for (auto& bl : bullets) {
            if (!bl.alive) continue;
            bulletGlow(bl);
            window.draw(bl.shape);
        }
        for (auto& B : balls) {
            ballGlow(B);
            window.draw(B.shape);
        }
        drawPaddle();
        drawHUD();
    }

    void drawBoss() {
        if (!boss.active) return;
        sf::FloatRect body = bossBodyRect();
        float bodyBottom = body.position.y + body.size.y;
        float labelTop = bodyBottom + BOSS_LABEL_GAP;

        sf::Color bodyCol = boss.phase == 1 ? sf::Color(95, 35, 115) :
            boss.phase == 2 ? sf::Color(130, 25, 65) :
            sf::Color(170, 15, 30);
        if (boss.flashTimer > 0.f) {
            float t = boss.flashTimer / BOSS_FLASH_DURATION;
            bodyCol = sf::Color(
                (uint8_t)(bodyCol.r + (255 - bodyCol.r) * t),
                (uint8_t)(bodyCol.g + (255 - bodyCol.g) * t),
                (uint8_t)(bodyCol.b + (255 - bodyCol.b) * t));
        }

        // Soft shadow under the boss body
        sf::RectangleShape shadow({ body.size.x + 6.f, body.size.y + 4.f });
        shadow.setOrigin({ (body.size.x + 6.f) * .5f, (body.size.y + 4.f) * .5f });
        shadow.setPosition({ body.position.x + body.size.x * .5f + 2.f, body.position.y + body.size.y * .5f + 3.f });
        shadow.setFillColor({ 0,0,0,70 });
        window.draw(shadow);

        sf::RectangleShape b({ body.size.x, body.size.y });
        b.setPosition(body.position);
        b.setFillColor(bodyCol);
        b.setOutlineThickness(2.5f);
        b.setOutlineColor(sf::Color(255, 255, 255, 100));
        window.draw(b);

        // Simple glowing eyes for character
        for (int s = -1; s <= 1; s += 2) {
            sf::Vector2f eyePos{ WIN_W * .5f + s * 70.f, body.position.y + body.size.y * .5f };
            sf::CircleShape glow(11.f);
            glow.setOrigin({ 11.f,11.f }); glow.setPosition(eyePos);
            glow.setFillColor(sf::Color(255, 230, 80, 70));
            window.draw(glow);
            sf::CircleShape eye(7.f);
            eye.setOrigin({ 7.f,7.f }); eye.setPosition(eyePos);
            eye.setFillColor(sf::Color(255, 230, 80));
            window.draw(eye);
            sf::CircleShape pupil(3.f);
            pupil.setOrigin({ 3.f,3.f }); pupil.setPosition(eyePos);
            pupil.setFillColor(sf::Color(25, 10, 10));
            window.draw(pupil);
        }

        // Boss HP pips + phase label, in the band between the boss body and
        // the shield (the boss now sits at the TOP, so this can't go above it)
        for (int i = 0; i < BOSS_MAX_HP; ++i) {
            sf::CircleShape pip(7.f);
            pip.setOrigin({ 7.f,7.f });
            pip.setPosition({ WIN_W * .5f - (BOSS_MAX_HP - 1) * 13.f + i * 26.f, labelTop + 8.f });
            pip.setFillColor(i < boss.hp ? sf::Color(255, 70, 90) : sf::Color(50, 50, 60));
            pip.setOutlineThickness(1.5f);
            pip.setOutlineColor(sf::Color(255, 255, 255, 130));
            window.draw(pip);
        }
        auto phaseT = txt("PHASE " + std::to_string(boss.phase), 12, sf::Color(255, 200, 220));
        centreOn(phaseT, WIN_W * .5f, labelTop + 19.f);
        window.draw(phaseT);

        // Indestructible shield platform -- small "materialize" animation
        // (scales up + fades in) whenever it (re)appears.
        sf::FloatRect sh = bossShieldRect();
        float shieldT = 1.f;
        uint8_t shieldAlphaMul = 255;
        if (boss.shieldSpawnTimer > 0.f && BOSS_SHIELD_SPAWN_TIME > 0.f) {
            float frac = boss.shieldSpawnTimer / BOSS_SHIELD_SPAWN_TIME; // 1 -> 0
            shieldT = std::max(0.05f, 1.f - frac);
            shieldAlphaMul = (uint8_t)(255.f * shieldT);
        }
        sf::Vector2f shCenter{ sh.position.x + sh.size.x * .5f, sh.position.y + sh.size.y * .5f };

        sf::RectangleShape shieldShadow({ sh.size.x + 4.f, sh.size.y + 3.f });
        shieldShadow.setOrigin({ (sh.size.x + 4.f) * .5f, (sh.size.y + 3.f) * .5f });
        shieldShadow.setPosition({ shCenter.x, shCenter.y + 2.f });
        shieldShadow.setScale({ shieldT, shieldT });
        shieldShadow.setFillColor({ 0,0,0,(uint8_t)(60.f * shieldT) });
        window.draw(shieldShadow);

        sf::RectangleShape shp({ sh.size.x, sh.size.y });
        shp.setOrigin({ sh.size.x * .5f, sh.size.y * .5f });
        shp.setPosition(shCenter);
        shp.setScale({ shieldT, shieldT });
        shp.setFillColor(sf::Color(120, 220, 255, shieldAlphaMul));
        shp.setOutlineThickness(2.f);
        shp.setOutlineColor(sf::Color(255, 255, 255, (uint8_t)(160.f * shieldT)));
        window.draw(shp);

        sf::RectangleShape shineLine({ sh.size.x - 6.f, 3.f });
        shineLine.setOrigin({ (sh.size.x - 6.f) * .5f, 1.5f });
        shineLine.setPosition(shCenter);
        shineLine.setScale({ shieldT, shieldT });
        shineLine.setFillColor({ 255,255,255,(uint8_t)(90.f * shieldT) });
        window.draw(shineLine);
    }

    void drawPaddle() {
        float hw = paddleW() * .5f;
        float py = WIN_H - 50.f;
        float hpF = (float)paddleHp / currentMaxHp();

        sf::Color pc;
        if (healFlash > 0.f) {
            float t = healFlash / .75f;
            pc = { (uint8_t)(75 + (int)(30 * t)), (uint8_t)(195 + (int)(60 * t)), (uint8_t)(75 + (int)(30 * t)) };
        }
        else if (hpF > 0.5f) pc = C_PADDLE;
        else if (hpF > 0.2f) pc = C_PADDLE_MD;
        else                    pc = C_PADDLE_LO;

        sf::RectangleShape shadow({ paddleW() + 4.f, PADDLE_H + 3.f });
        shadow.setOrigin({ (paddleW() + 4.f) * .5f, (PADDLE_H + 3.f) * .5f });
        shadow.setPosition({ paddleX + 2.f, py + 3.f });
        shadow.setFillColor({ 0,0,0,60 });
        window.draw(shadow);

        sf::RectangleShape p({ paddleW(), PADDLE_H });
        p.setOrigin({ hw, PADDLE_H * .5f });
        p.setPosition({ paddleX, py });
        p.setFillColor(pc);
        p.setOutlineThickness(1.5f);
        p.setOutlineColor({ 255,255,255,55 });
        window.draw(p);

        sf::RectangleShape hl({ paddleW() - 6.f, 3.5f });
        hl.setOrigin({ (paddleW() - 6.f) * .5f, 1.75f });
        hl.setPosition({ paddleX, py - PADDLE_H * .5f + 3.f });
        hl.setFillColor({ 255,255,255,72 });
        window.draw(hl);
    }

    sf::Text txt(const std::string& s, unsigned sz, sf::Color c = sf::Color::White) {
        sf::Text t(font, s, sz); t.setFillColor(c); return t;
    }

    void drawHUD() {
        {
            auto t = txt("SCORE  " + std::to_string(score), 20);
            auto b = t.getLocalBounds();
            t.setPosition({ WIN_W * .5f - (b.position.x + b.size.x * .5f),10.f });
            window.draw(t);
        }
        {
            auto t = txt("BEST  " + std::to_string(hiScore), 14, { 165,165,165 });
            auto b = t.getLocalBounds();
            t.setPosition({ WIN_W - (b.position.x + b.size.x) - 12.f,12.f });
            window.draw(t);
        }
        {
            auto t = txt("GOLD  " + std::to_string(gold), 14, C_GOLD);
            auto b = t.getLocalBounds();
            t.setPosition({ WIN_W - (b.position.x + b.size.x) - 12.f,30.f });
            window.draw(t);
        }
        {
            auto t = txt(boss.active ? "STAGE " + std::to_string(stageNum + 1) + " - BOSS" : "STAGE " + std::to_string(stageNum + 1),
                15, boss.active ? sf::Color(255, 140, 160) : sf::Color(145, 158, 222));
            t.setPosition({ 10.f,13.f }); window.draw(t);
        }
        {
            auto t = txt("[E] CLEAR x" + std::to_string(bulletClearCharges), 13,
                bulletClearCharges > 0 ? sf::Color(255, 200, 120) : sf::Color(90, 90, 90));
            t.setPosition({ 10.f,33.f }); window.draw(t);
        }
        if (GOD_MODE_CHEAT_ENABLED) {
            auto t = txt("[F1] GOD MODE: CLEAR BRICKS", 11, sf::Color(255, 90, 200, 180));
            t.setPosition({ 10.f,52.f }); window.draw(t);
        }

        float px = 10.f, py = (float)WIN_H - 22.f;
        for (int i = 0;i < HEAL_HITS_NEEDED;++i) {
            sf::RectangleShape pip({ 9.f,9.f });
            pip.setFillColor(i < comboHits ? C_HEAL : sf::Color{ 32,32,42 });
            pip.setOutlineThickness(1.f);
            pip.setOutlineColor({ 75,75,95 });
            pip.setPosition({ px + i * 13.f, py });
            window.draw(pip);
        }
        auto lbl = txt("COMBO", 11, { 85,155,85 });
        lbl.setPosition({ px + HEAL_HITS_NEEDED * 13.f + 5.f,py + 1.f });
        window.draw(lbl);

        if (!ballLaunched && state == State::PLAYING) {
            auto t = txt("SPACE to launch", 15, { 155,155,198 });
            centreOn(t, WIN_W * .5f, (float)WIN_H - 18.f); window.draw(t);
        }
        if (clearFlashTimer > 0.f) {
            auto t = txt("BULLETS CLEARED!", 22, sf::Color(255, 210, 90));
            centreOn(t, WIN_W * .5f, 90.f); window.draw(t);
        }
    }

    void fillScreen(sf::Color c) {
        sf::RectangleShape r({ (float)WIN_W,(float)WIN_H });
        r.setFillColor(c); window.draw(r);
    }

    void glowTxt(const std::string& s, unsigned sz, sf::Color col, float x, float y, bool bold = false) {
        static const int OFF[][2] = { {-3,0},{3,0},{0,-3},{0,3},{-2,-2},{2,-2},{-2,2},{2,2} };
        sf::Text sh(font, s, sz); sh.setFillColor({ 0,0,0,105 });
        if (bold)sh.setStyle(sf::Text::Bold);
        centreOn(sh, x + 2.f, y + 2.f); window.draw(sh);
        for (auto& o : OFF) {
            sf::Text g(font, s, sz); g.setFillColor({ col.r,col.g,col.b,48 });
            if (bold)g.setStyle(sf::Text::Bold);
            centreOn(g, x + (float)o[0], y + (float)o[1]); window.draw(g);
        }
        sf::Text t(font, s, sz); t.setFillColor(col);
        if (bold)t.setStyle(sf::Text::Bold);
        centreOn(t, x, y); window.draw(t);
    }

    void hRule(float y, float alpha = 25.f) {
        sf::RectangleShape l({ (float)WIN_W,1.5f });
        l.setPosition({ 0.f,y });
        l.setFillColor({ 255,255,255,(uint8_t)alpha }); window.draw(l);
    }

    // Generic button drawer. Returns nothing; just renders.
    void drawButton(sf::FloatRect r, const std::string& label, sf::Vector2f mouse,
        sf::Color base, bool enabled = true, unsigned fontSize = 20) {
        bool hov = enabled && hit(r, mouse);
        sf::RectangleShape b({ r.size.x, r.size.y });
        b.setPosition(r.position);
        sf::Color fill = enabled ? (hov ? lighten(base, 30) : base) : sf::Color(55, 55, 65);
        b.setFillColor(fill);
        b.setOutlineThickness(2.f);
        b.setOutlineColor(enabled ? sf::Color(255, 255, 255, hov ? 170 : 90) : sf::Color(255, 255, 255, 40));
        window.draw(b);

        sf::RectangleShape shine({ r.size.x - 6.f, 4.f });
        shine.setPosition({ r.position.x + 3.f, r.position.y + 3.f });
        shine.setFillColor({ 255,255,255, enabled ? (uint8_t)40 : (uint8_t)15 });
        window.draw(shine);

        auto t = txt(label, fontSize, enabled ? sf::Color::White : sf::Color(150, 150, 150));
        centreOn(t, r.position.x + r.size.x * .5f, r.position.y + r.size.y * .5f);
        window.draw(t);
    }

    // ---- START (3-button main menu) ----------------------------------
    void drawStart(sf::Vector2f mp) {
        for (int row = 0; row < 2; ++row)
            for (int c = 0; c < BRICK_COLS; ++c) {
                float bx = BRICK_OFF_X + c * (BRICK_W + BRICK_PAD);
                float by = 18.f + row * (BRICK_H + BRICK_PAD);
                sf::RectangleShape d({ BRICK_W,BRICK_H });
                d.setPosition({ bx,by }); d.setFillColor(BRICK_COL[(c + row * 2) % BRICK_ROWS]);
                d.setOutlineThickness(1.f); d.setOutlineColor({ 0,0,0,50 });
                window.draw(d);
                sf::RectangleShape sh({ BRICK_W - 6.f,3.f });
                sh.setPosition({ bx + 3.f,by + 2.5f }); sh.setFillColor({ 255,255,255,52 });
                window.draw(sh);
            }
        hRule(76.f, 20.f);

        glowTxt("BRICK", 60, { 100,215,255 }, WIN_W * .5f - 98.f, 128.f, true);
        glowTxt("BREAKER", 60, { 255,238,95 }, WIN_W * .5f + 108.f, 128.f, true);

        {
            auto t = txt("Dodge bullets.  Break bricks.  Buy upgrades.", 17, { 182,182,208 });
            centreOn(t, WIN_W * .5f, 178.f); window.draw(t);
        }

        hRule(206.f, 18.f);

        if (hiScore > 0) {
            auto h = txt("BEST SCORE  " + std::to_string(hiScore), 16, C_GOLD);
            centreOn(h, WIN_W * .5f, 232.f); window.draw(h);
        }

        drawButton(playBtnRect(), "PLAY", mp, sf::Color(70, 170, 90));
        drawButton(settingsBtnRect(), "SETTINGS", mp, sf::Color(70, 110, 190));
        drawButton(exitBtnRect(), "EXIT", mp, sf::Color(190, 70, 70));

        {
            auto f = txt("Click a button to begin", 13, { 115,115,145 });
            centreOn(f, WIN_W * .5f, 520.f); window.draw(f);
        }
    }

    // ---- SETTINGS ------------------------------------------------------
    void drawSettings(sf::Vector2f mp) {
        glowTxt("SETTINGS", 46, { 120,190,255 }, WIN_W * .5f, 68.f, true);
        hRule(104.f, 18.f);

        sf::RectangleShape card({ 440.f,182.f });
        card.setPosition({ WIN_W * .5f - 220.f,128.f });
        card.setFillColor(C_PANEL);
        card.setOutlineThickness(1.f); card.setOutlineColor(C_PANEL_OUT);
        window.draw(card);

        auto krow = [&](float y, const std::string& k, const std::string& v) {
            sf::Text kt(font, k, 15); kt.setFillColor({ 255,232,88 });
            kt.setPosition({ WIN_W * .5f - 205.f,y }); window.draw(kt);
            sf::Text vt(font, v, 15); vt.setFillColor({ 202,202,222 });
            vt.setPosition({ WIN_W * .5f - 25.f,y }); window.draw(vt);
            };
        krow(140.f, "A / Left  D / Right", "Move paddle");
        krow(167.f, "SPACE", "Launch ball");
        krow(194.f, "P / ESC", "Pause game");
        krow(221.f, "E", "Clear all bullets (consumable)");
        krow(248.f, std::to_string(HEAL_HITS_NEEDED) + " hits in a row", "Restore 1 HP");
        krow(275.f, "Bullets", "Type/speed/angle depend on brick colour");
        krow(295.f, "Interest", "Capped -- buy Bank Capacity to raise it");

        {
            auto t = txt("MASTER VOLUME", 16, sf::Color(200, 200, 220));
            centreOn(t, WIN_W * .5f, 332.f); window.draw(t);
        }
        auto sr = volumeSliderRect();
        sf::RectangleShape track({ sr.size.x, sr.size.y });
        track.setPosition(sr.position);
        track.setFillColor({ 50,50,65 });
        track.setOutlineThickness(1.f); track.setOutlineColor({ 255,255,255,40 });
        window.draw(track);

        float fillW = sr.size.x * (masterVolume / 100.f);
        sf::RectangleShape fillBar({ fillW, sr.size.y });
        fillBar.setPosition(sr.position);
        fillBar.setFillColor({ 100,200,255 });
        window.draw(fillBar);

        sf::CircleShape knob(9.f);
        knob.setOrigin({ 9.f,9.f });
        knob.setPosition({ sr.position.x + fillW, sr.position.y + sr.size.y * .5f });
        knob.setFillColor(sf::Color::White);
        knob.setOutlineThickness(2.f); knob.setOutlineColor({ 100,170,220 });
        window.draw(knob);

        {
            auto t = txt(std::to_string((int)masterVolume) + "%", 15, sf::Color(220, 220, 235));
            centreOn(t, WIN_W * .5f, 355.f); window.draw(t);
        }

        {
            auto note = txt("(Wire to SFML Audio module to affect real sound)", 12, sf::Color(110, 110, 135));
            centreOn(note, WIN_W * .5f, 380.f); window.draw(note);
        }

        drawButton(settingsBackRect(), "BACK", mp, sf::Color(80, 80, 100));
    }

    // ---- PAUSE -----------------------------------------------------
    void drawPauseOverlay(sf::Vector2f mp) {
        fillScreen(C_OVERLAY);
        glowTxt("PAUSED", 54, { 100,200,255 }, WIN_W * .5f, 170.f, true);
        drawButton(pauseResumeRect(), "RESUME", mp, sf::Color(70, 170, 90));
        drawButton(pauseRestartRect(), "RESTART", mp, sf::Color(190, 140, 50));
        drawButton(pauseMenuRect(), "MAIN MENU", mp, sf::Color(120, 80, 180));
    }

    // ---- STAGE CLEARED ------------------------------------------------
    void drawStageClearedOverlay(sf::Vector2f mp) {
        fillScreen(lastStageWasBoss ? sf::Color(24, 4, 4, 200) : sf::Color(0, 16, 0, 190));
        std::string title = lastStageWasBoss ? "BOSS DEFEATED!" : ("STAGE " + std::to_string(stageNum + 1) + " CLEARED!");
        sf::Color titleCol = lastStageWasBoss ? sf::Color(255, 90, 110) : C_HEAL;
        glowTxt(title, 38, titleCol, WIN_W * .5f, 128.f, true);

        {
            auto t = txt("+" + std::to_string(lastReward) + " gold earned" +
                (lastStageWasBoss ? "  (incl. +" + std::to_string(BOSS_DEFEAT_BONUS_GOLD) + " boss bonus)" : ""),
                20, sf::Color(255, 240, 170));
            centreOn(t, WIN_W * .5f, 202.f); window.draw(t);
        }
        {
            std::string s = lastInterest > 0 ?
                "+" + std::to_string(lastInterest) + " interest (on saved gold)" :
                "No interest (save 10+ gold to earn some)";
            auto t = txt(s, 16, sf::Color(180, 210, 255));
            centreOn(t, WIN_W * .5f, 232.f); window.draw(t);
        }
        {
            auto t = txt("Interest cap: " + std::to_string(interestCap()) + " gold saved -> max " +
                std::to_string((int)(std::floor(interestCap() / 10.f) * GOLD_INTEREST_PER_10)) + " interest",
                13, sf::Color(140, 160, 190));
            centreOn(t, WIN_W * .5f, 258.f); window.draw(t);
        }
        {
            auto t = txt("Total Gold: " + std::to_string(gold), 22, C_GOLD);
            centreOn(t, WIN_W * .5f, 302.f); window.draw(t);
        }
        {
            auto t = txt("Score: " + std::to_string(score), 17, sf::Color(220, 220, 220));
            centreOn(t, WIN_W * .5f, 336.f); window.draw(t);
        }

        drawButton(stageContinueRect(), "CONTINUE TO SHOP", mp, sf::Color(70, 170, 90), true, 19);
    }

    // ---- SHOP -----------------------------------------------------
    void drawPowerupCard(int idx, const Powerup& p, sf::Vector2f mp) {
        float y = 200.f + idx * 100.f;
        sf::RectangleShape card({ 560.f, 84.f });
        card.setPosition({ WIN_W * .5f - 280.f, y });
        card.setFillColor(C_PANEL);
        card.setOutlineThickness(1.5f);
        card.setOutlineColor(p.maxed() ? sf::Color(120, 200, 140, 90) : C_PANEL_OUT);
        window.draw(card);

        sf::RectangleShape strip({ 6.f,84.f });
        strip.setPosition({ WIN_W * .5f - 280.f,y });
        strip.setFillColor(p.accent);
        window.draw(strip);

        auto name = txt(p.name, 19, p.accent);
        name.setPosition({ WIN_W * .5f - 258.f, y + 10.f });
        window.draw(name);

        auto descT = txt(p.desc, 12, sf::Color(185, 185, 205));
        descT.setPosition({ WIN_W * .5f - 258.f, y + 36.f });
        window.draw(descT);

        std::string lvlStr = p.maxed() ?
            "MAXED (" + std::to_string(p.level) + "/" + std::to_string(p.levelCap) + ")" :
            "Level " + std::to_string(p.level) + "/" + std::to_string(p.levelCap);
        auto lvlT = txt(lvlStr, 13, p.maxed() ? C_HEAL : sf::Color(150, 150, 175));
        lvlT.setPosition({ WIN_W * .5f - 258.f, y + 58.f });
        window.draw(lvlT);

        bool canAfford = !p.maxed() && gold >= p.price();
        std::string label = p.maxed() ? "MAXED" : (std::to_string(p.price()) + " G");
        drawButton(shopBuyRect(idx), label, mp, canAfford ? sf::Color(70, 150, 200) : sf::Color(90, 60, 60),
            !p.maxed(), 17);
    }

    void drawShopOverlay(sf::Vector2f mp) {
        fillScreen({ 6,6,18,225 });
        glowTxt("SHOP", 38, { 255,238,95 }, WIN_W * .5f, 52.f, true);
        {
            auto t = txt("Gold: " + std::to_string(gold), 19, C_GOLD);
            centreOn(t, WIN_W * .5f, 90.f); window.draw(t);
        }
        hRule(114.f, 18.f);

        for (int i = 0;i < 3;++i)
            drawPowerupCard(i, powerups[shopOffer[i]], mp);

        drawButton(shopNextRect(), "NEXT STAGE  ->", mp, sf::Color(70, 170, 90), true, 20);

        {
            auto t = txt("3 random powerups offered each visit  |  unspent gold earns interest up to the cap",
                12, sf::Color(120, 120, 150));
            centreOn(t, WIN_W * .5f, 600.f - 14.f); window.draw(t);
        }
    }

    // ---- GAME OVER ------------------------------------------------
    void drawGameOverOverlay(sf::Vector2f mp) {
        fillScreen({ 18,0,0,192 });
        glowTxt("GAME  OVER", 54, { 255,62,62 }, WIN_W * .5f, 140.f, true);
        {
            auto s = txt("Score:  " + std::to_string(score), 26);
            centreOn(s, WIN_W * .5f, 220.f); window.draw(s);
            auto h = txt("Best:   " + std::to_string(hiScore), 20, C_GOLD);
            centreOn(h, WIN_W * .5f, 256.f); window.draw(h);
            auto g = txt("Stage reached: " + std::to_string(stageNum + 1), 18, sf::Color(200, 200, 220));
            centreOn(g, WIN_W * .5f, 288.f); window.draw(g);
        }

        drawButton(goPlayAgainRect(), "PLAY AGAIN", mp, sf::Color(70, 170, 90));
        drawButton(goMenuRect(), "MAIN MENU", mp, sf::Color(120, 80, 180));
    }
};

// ================================================================
int main() { Game g; g.run(); }
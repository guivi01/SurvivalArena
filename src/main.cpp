#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr unsigned WindowWidth = 1280;
constexpr unsigned WindowHeight = 720;
constexpr float PlayerRadius = 18.0f;
constexpr float EnemyRadius = 16.0f;
constexpr float PickupRadius = 12.0f;
constexpr float ArenaPadding = 64.0f;
constexpr int PlayerSpriteColumns = 4;
constexpr int PlayerSpriteRows = 2;
constexpr int PlayerWeaponSpriteCount = 3;
constexpr float PlayerSpriteHeight = 86.0f;
constexpr int PickupIconCount = 5;
constexpr float PickupIconSize = 48.0f;
constexpr int EnemySpriteColumns = 4;
constexpr int EnemySpriteRows = 2;
constexpr int EnemySpriteFrameCount = EnemySpriteColumns * EnemySpriteRows;
constexpr float EnemySpriteHeight = 74.0f;

float length(sf::Vector2f value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

sf::Vector2f normalize(sf::Vector2f value) {
    const float len = length(value);
    if (len == 0.0f) {
        return {0.0f, 0.0f};
    }
    return value / len;
}

float distance(sf::Vector2f a, sf::Vector2f b) {
    return length(a - b);
}

float angleDegrees(sf::Vector2f direction) {
    return std::atan2(direction.y, direction.x) * 180.0f / 3.14159265f;
}

float clamp(float value, float low, float high) {
    return std::max(low, std::min(value, high));
}

void maskLightCheckerboard(sf::Image& image) {
    const sf::Vector2u imageSize = image.getSize();
    const unsigned width = imageSize.x;
    const unsigned height = imageSize.y;
    std::vector<bool> background(width * height, false);
    std::vector<unsigned> stack;
    stack.reserve(width * height / 4);

    auto index = [width](unsigned x, unsigned y) {
        return y * width + x;
    };

    auto isCheckerPixel = [](sf::Color pixel) {
        const int brightest = std::max({pixel.r, pixel.g, pixel.b});
        const int darkest = std::min({pixel.r, pixel.g, pixel.b});
        return darkest > 170 && brightest - darkest < 42;
    };

    auto addSeed = [&](unsigned x, unsigned y) {
        const unsigned i = index(x, y);
        if (!background[i] && isCheckerPixel(image.getPixel(x, y))) {
            background[i] = true;
            stack.push_back(i);
        }
    };

    for (unsigned x = 0; x < width; ++x) {
        addSeed(x, 0);
        addSeed(x, height - 1);
    }
    for (unsigned y = 0; y < height; ++y) {
        addSeed(0, y);
        addSeed(width - 1, y);
    }

    while (!stack.empty()) {
        const unsigned current = stack.back();
        stack.pop_back();

        const unsigned x = current % width;
        const unsigned y = current / width;
        const std::array<sf::Vector2i, 4> neighbors = {
            sf::Vector2i{-1, 0},
            sf::Vector2i{1, 0},
            sf::Vector2i{0, -1},
            sf::Vector2i{0, 1}
        };

        for (const auto& offset : neighbors) {
            const int nx = static_cast<int>(x) + offset.x;
            const int ny = static_cast<int>(y) + offset.y;
            if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) || ny >= static_cast<int>(height)) {
                continue;
            }

            const unsigned neighborIndex = index(static_cast<unsigned>(nx), static_cast<unsigned>(ny));
            if (!background[neighborIndex] && isCheckerPixel(image.getPixel(static_cast<unsigned>(nx), static_cast<unsigned>(ny)))) {
                background[neighborIndex] = true;
                stack.push_back(neighborIndex);
            }
        }
    }

    std::vector<sf::Uint8> alpha(width * height, 255);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const unsigned i = index(x, y);
            if (background[i]) {
                alpha[i] = 0;
                continue;
            }

            bool touchesBackground = false;
            for (int oy = -2; oy <= 2 && !touchesBackground; ++oy) {
                for (int ox = -2; ox <= 2 && !touchesBackground; ++ox) {
                    const int nx = static_cast<int>(x) + ox;
                    const int ny = static_cast<int>(y) + oy;
                    if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) || ny >= static_cast<int>(height)) {
                        continue;
                    }
                    touchesBackground = background[index(static_cast<unsigned>(nx), static_cast<unsigned>(ny))];
                }
            }

            if (!touchesBackground) {
                continue;
            }

            const sf::Color pixel = image.getPixel(x, y);
            const int brightest = std::max({pixel.r, pixel.g, pixel.b});
            const int darkest = std::min({pixel.r, pixel.g, pixel.b});
            const int spread = brightest - darkest;
            const float brightnessBlend = clamp((static_cast<float>(brightest) - 145.0f) / 105.0f, 0.0f, 1.0f);
            const float neutralBlend = clamp((65.0f - static_cast<float>(spread)) / 65.0f, 0.0f, 1.0f);
            const float backgroundBlend = brightnessBlend * neutralBlend;
            alpha[i] = static_cast<sf::Uint8>(255.0f * (1.0f - backgroundBlend * 0.86f));
        }
    }

    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            sf::Color pixel = image.getPixel(x, y);
            pixel.a = alpha[index(x, y)];
            image.setPixel(x, y, pixel);
        }
    }
}

sf::IntRect visibleBounds(const sf::Image& image, const sf::IntRect& cell) {
    int minX = cell.left + cell.width;
    int minY = cell.top + cell.height;
    int maxX = cell.left;
    int maxY = cell.top;

    for (int y = cell.top; y < cell.top + cell.height; ++y) {
        for (int x = cell.left; x < cell.left + cell.width; ++x) {
            if (image.getPixel(static_cast<unsigned>(x), static_cast<unsigned>(y)).a == 0) {
                continue;
            }

            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (minX > maxX || minY > maxY) {
        return cell;
    }

    return {minX, minY, maxX - minX + 1, maxY - minY + 1};
}

struct Weapon {
    std::string name;
    float fireDelay;
    float bulletSpeed;
    int damage;
    int pellets;
    float spreadDegrees;
    int ammoCost;
    sf::Color color;
};

const std::vector<Weapon> Weapons = {
    {"Pistol", 0.22f, 760.0f, 1, 1, 0.0f, 1, sf::Color(250, 238, 140)},
    {"Scatter", 0.65f, 640.0f, 1, 5, 24.0f, 2, sf::Color(255, 155, 95)},
    {"Rifle", 0.10f, 900.0f, 1, 1, 0.0f, 1, sf::Color(150, 220, 255)},
};

struct Bullet {
    sf::Vector2f position;
    sf::Vector2f velocity;
    float life;
    int damage;
    sf::Color color;
};

struct Enemy {
    sf::Vector2f position;
    sf::Vector2f velocity;
    float speed;
    int health;
    int maxHealth;
    float animationTime;
};

enum class PickupType {
    Ammo,
    Health,
    Weapon
};

struct Pickup {
    PickupType type;
    sf::Vector2f position;
    int amount;
    int weaponIndex;
    float pulse;
};

struct Player {
    sf::Vector2f position{WindowWidth / 2.0f, WindowHeight / 2.0f};
    sf::Vector2f aim{1.0f, 0.0f};
    int health = 100;
    int ammo = 80;
    int weaponIndex = 0;
    float shootCooldown = 0.0f;
    float invulnerable = 0.0f;
};

class Game {
public:
    Game()
        : window_(sf::VideoMode(WindowWidth, WindowHeight), "Survival Arena"),
          rng_(std::random_device{}()) {
        window_.setFramerateLimit(120);
        font_.loadFromFile("assets/fonts/DejaVuSans.ttf");
        loadBackground();
        loadPlayerSprite();
        loadPickupIcons();
        loadEnemySprite();
        startBackgroundMusic();
        reset();
    }

    void run() {
        sf::Clock clock;
        while (window_.isOpen()) {
            const float dt = std::min(clock.restart().asSeconds(), 1.0f / 30.0f);
            handleEvents();
            if (!paused_ && !gameOver_) {
                update(dt);
            }
            render();
        }
    }

private:
    void reset() {
        player_ = Player{};
        bullets_.clear();
        enemies_.clear();
        pickups_.clear();
        elapsed_ = 0.0f;
        score_ = 0;
        wave_ = 0;
        enemiesToSpawn_ = 0;
        spawnTimer_ = 1.2f;
        pickupTimer_ = 4.0f;
        waveBreak_ = 1.0f;
        paused_ = false;
        gameOver_ = false;
    }

    void handleEvents() {
        sf::Event event{};
        while (window_.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window_.close();
            }
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    window_.close();
                }
                if (event.key.code == sf::Keyboard::P && !gameOver_) {
                    paused_ = !paused_;
                }
                if (event.key.code == sf::Keyboard::R && gameOver_) {
                    reset();
                }
            }
        }
    }

    void update(float dt) {
        elapsed_ += dt;
        score_ += static_cast<int>(dt * 8.0f);

        updatePlayer(dt);
        updateWaves(dt);
        updateBullets(dt);
        updateEnemies(dt);
        updatePickups(dt);
        cleanupEntities();
    }

    void updatePlayer(float dt) {
        sf::Vector2f movement{0.0f, 0.0f};
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
            movement.y -= 1.0f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
            movement.y += 1.0f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
            movement.x -= 1.0f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
            movement.x += 1.0f;
        }

        const float speed = 285.0f;
        const sf::Vector2f movementDirection = normalize(movement);
        player_.position += movementDirection * speed * dt;
        player_.position.x = clamp(player_.position.x, ArenaPadding, WindowWidth - ArenaPadding);
        player_.position.y = clamp(player_.position.y, ArenaPadding, WindowHeight - ArenaPadding);
        updatePlayerAnimation(movementDirection, dt);

        const sf::Vector2i mousePixel = sf::Mouse::getPosition(window_);
        const sf::Vector2f mouseWorld = window_.mapPixelToCoords(mousePixel);
        player_.aim = normalize(mouseWorld - player_.position);
        if (length(player_.aim) == 0.0f) {
            player_.aim = {1.0f, 0.0f};
        }

        player_.shootCooldown = std::max(0.0f, player_.shootCooldown - dt);
        player_.invulnerable = std::max(0.0f, player_.invulnerable - dt);
        if ((sf::Mouse::isButtonPressed(sf::Mouse::Left) || sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
            && player_.shootCooldown <= 0.0f) {
            shoot();
        }
    }

    void shoot() {
        const Weapon& weapon = Weapons[player_.weaponIndex];
        if (player_.ammo < weapon.ammoCost) {
            player_.shootCooldown = 0.18f;
            return;
        }

        player_.ammo -= weapon.ammoCost;
        player_.shootCooldown = weapon.fireDelay;

        const float baseAngle = angleDegrees(player_.aim);
        const float firstOffset = weapon.pellets == 1 ? 0.0f : -weapon.spreadDegrees * 0.5f;
        const float step = weapon.pellets == 1 ? 0.0f : weapon.spreadDegrees / static_cast<float>(weapon.pellets - 1);

        for (int i = 0; i < weapon.pellets; ++i) {
            const float angle = (baseAngle + firstOffset + step * static_cast<float>(i)) * 3.14159265f / 180.0f;
            const sf::Vector2f dir{std::cos(angle), std::sin(angle)};
            bullets_.push_back({
                player_.position + dir * (PlayerRadius + 10.0f),
                dir * weapon.bulletSpeed,
                1.1f,
                weapon.damage,
                weapon.color
            });
        }
    }

    void updateWaves(float dt) {
        if (enemiesToSpawn_ <= 0 && enemies_.empty()) {
            waveBreak_ -= dt;
            if (waveBreak_ <= 0.0f) {
                ++wave_;
                enemiesToSpawn_ = 5 + wave_ * 3;
                spawnTimer_ = 0.2f;
                waveBreak_ = std::max(1.0f, 3.0f - wave_ * 0.08f);
            }
        }

        if (enemiesToSpawn_ > 0) {
            spawnTimer_ -= dt;
            const float spawnDelay = std::max(0.12f, 0.85f - wave_ * 0.035f);
            if (spawnTimer_ <= 0.0f) {
                spawnEnemy();
                --enemiesToSpawn_;
                spawnTimer_ = spawnDelay;
            }
        }
    }

    void spawnEnemy() {
        std::uniform_int_distribution<int> sideDist(0, 3);
        std::uniform_real_distribution<float> xDist(40.0f, WindowWidth - 40.0f);
        std::uniform_real_distribution<float> yDist(40.0f, WindowHeight - 40.0f);

        sf::Vector2f position{};
        switch (sideDist(rng_)) {
            case 0: position = {xDist(rng_), -EnemyRadius}; break;
            case 1: position = {WindowWidth + EnemyRadius, yDist(rng_)}; break;
            case 2: position = {xDist(rng_), WindowHeight + EnemyRadius}; break;
            default: position = {-EnemyRadius, yDist(rng_)}; break;
        }

        const int health = 1 + wave_ / 4;
        const float speed = 95.0f + static_cast<float>(wave_) * 6.0f;
        enemies_.push_back({position, {0.0f, 0.0f}, speed, health, health, 0.0f});
    }

    void updateBullets(float dt) {
        for (auto& bullet : bullets_) {
            bullet.position += bullet.velocity * dt;
            bullet.life -= dt;
        }

        for (auto& bullet : bullets_) {
            if (bullet.life <= 0.0f) {
                continue;
            }

            for (auto& enemy : enemies_) {
                if (enemy.health <= 0) {
                    continue;
                }
                if (distance(bullet.position, enemy.position) < EnemyRadius + 5.0f) {
                    enemy.health -= bullet.damage;
                    bullet.life = 0.0f;
                    if (enemy.health <= 0) {
                        score_ += 50 + wave_ * 8;
                        maybeDropPickup(enemy.position);
                    }
                    break;
                }
            }
        }
    }

    void updateEnemies(float dt) {
        for (auto& enemy : enemies_) {
            const sf::Vector2f dir = normalize(player_.position - enemy.position);
            enemy.velocity = dir * enemy.speed;
            enemy.position += enemy.velocity * dt;
            enemy.animationTime += dt;

            if (player_.invulnerable <= 0.0f && distance(enemy.position, player_.position) < EnemyRadius + PlayerRadius) {
                player_.health -= 12;
                player_.invulnerable = 0.65f;
                enemy.position -= dir * 28.0f;
                if (player_.health <= 0) {
                    player_.health = 0;
                    gameOver_ = true;
                }
            }
        }
    }

    void updatePickups(float dt) {
        pickupTimer_ -= dt;
        if (pickupTimer_ <= 0.0f) {
            spawnRandomPickup();
            pickupTimer_ = std::max(2.0f, 6.0f - wave_ * 0.08f);
        }

        for (auto& pickup : pickups_) {
            pickup.pulse += dt * 5.0f;
            if (distance(pickup.position, player_.position) < PickupRadius + PlayerRadius) {
                collect(pickup);
                pickup.amount = 0;
            }
        }
    }

    void spawnRandomPickup() {
        std::uniform_real_distribution<float> xDist(ArenaPadding + 35.0f, WindowWidth - ArenaPadding - 35.0f);
        std::uniform_real_distribution<float> yDist(ArenaPadding + 35.0f, WindowHeight - ArenaPadding - 35.0f);
        std::uniform_int_distribution<int> typeDist(0, 9);

        const int roll = typeDist(rng_);
        if (roll < 5) {
            pickups_.push_back({PickupType::Ammo, {xDist(rng_), yDist(rng_)}, 30, 0, 0.0f});
        } else if (roll < 8) {
            pickups_.push_back({PickupType::Health, {xDist(rng_), yDist(rng_)}, 25, 0, 0.0f});
        } else {
            std::uniform_int_distribution<int> weaponDist(1, static_cast<int>(Weapons.size()) - 1);
            pickups_.push_back({PickupType::Weapon, {xDist(rng_), yDist(rng_)}, 1, weaponDist(rng_), 0.0f});
        }
    }

    void maybeDropPickup(sf::Vector2f position) {
        std::uniform_int_distribution<int> dropDist(0, 99);
        const int roll = dropDist(rng_);
        if (roll < 12) {
            pickups_.push_back({PickupType::Ammo, position, 18, 0, 0.0f});
        } else if (roll < 18) {
            pickups_.push_back({PickupType::Health, position, 15, 0, 0.0f});
        }
    }

    void collect(const Pickup& pickup) {
        switch (pickup.type) {
            case PickupType::Ammo:
                player_.ammo = std::min(player_.ammo + pickup.amount, 240);
                score_ += 15;
                break;
            case PickupType::Health:
                player_.health = std::min(player_.health + pickup.amount, 100);
                score_ += 20;
                break;
            case PickupType::Weapon:
                player_.weaponIndex = pickup.weaponIndex;
                player_.ammo = std::min(player_.ammo + 40, 240);
                score_ += 75;
                break;
        }
    }

    void cleanupEntities() {
        bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet& bullet) {
            return bullet.life <= 0.0f || bullet.position.x < -80.0f || bullet.position.x > WindowWidth + 80.0f
                || bullet.position.y < -80.0f || bullet.position.y > WindowHeight + 80.0f;
        }), bullets_.end());

        enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& enemy) {
            return enemy.health <= 0;
        }), enemies_.end());

        pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& pickup) {
            return pickup.amount <= 0;
        }), pickups_.end());
    }

    void render() {
        window_.clear(sf::Color(16, 18, 24));
        drawBackground();
        drawArena();
        drawPickups();
        drawBullets();
        drawEnemies();
        drawPlayer();
        drawHud();
        if (paused_) {
            drawCenterText("PAUSED", "Press P to continue");
        }
        if (gameOver_) {
            drawCenterText("GAME OVER", "Press R to restart or Esc to quit");
        }
        window_.display();
    }

    void loadBackground() {
        const std::vector<std::string> paths = {
            "assets/Background.png",
            "../assets/Background.png"
        };

        for (const auto& path : paths) {
            if (backgroundTexture_.loadFromFile(path)) {
                backgroundSprite_.setTexture(backgroundTexture_);

                const sf::Vector2u textureSize = backgroundTexture_.getSize();
                const float scale = std::max(
                    WindowWidth / static_cast<float>(textureSize.x),
                    WindowHeight / static_cast<float>(textureSize.y));
                backgroundSprite_.setScale(scale, scale);

                const sf::FloatRect bounds = backgroundSprite_.getGlobalBounds();
                backgroundSprite_.setPosition(
                    (WindowWidth - bounds.width) * 0.5f,
                    (WindowHeight - bounds.height) * 0.5f);

                hasBackground_ = true;
                return;
            }
        }
    }

    void drawBackground() {
        if (hasBackground_) {
            window_.draw(backgroundSprite_);
            return;
        }

        sf::RectangleShape fallback({static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)});
        fallback.setFillColor(sf::Color(25, 29, 39));
        window_.draw(fallback);
    }

    void loadPlayerSprite() {
        const std::array<std::string, PlayerWeaponSpriteCount> filenames = {
            "Player_pistol.png",
            "Player_shotgun.png",
            "Player_machinegun.png"
        };

        for (int i = 0; i < PlayerWeaponSpriteCount; ++i) {
            loadPlayerTexture(i, filenames[i]);
        }
    }

    void loadPlayerTexture(int weaponIndex, const std::string& filename) {
        const std::vector<std::string> paths = {
            "assets/" + filename,
            "../assets/" + filename
        };

        sf::Image playerImage;
        for (const auto& path : paths) {
            if (playerImage.loadFromFile(path)) {
                const sf::Vector2u imageSize = playerImage.getSize();
                playerFrameSize_ = {
                    static_cast<int>(imageSize.x / PlayerSpriteColumns),
                    static_cast<int>(imageSize.y / PlayerSpriteRows)
                };

                maskLightCheckerboard(playerImage);

                if (playerTextures_[weaponIndex].loadFromImage(playerImage)) {
                    playerTextures_[weaponIndex].setSmooth(true);
                    hasPlayerSprites_[weaponIndex] = true;
                }
                return;
            }
        }
    }

    void loadPickupIcons() {
        const std::array<std::string, PickupIconCount> filenames = {
            "ammo.png",
            "health.png",
            "pistol.png",
            "shotgun.png",
            "machinegun.png"
        };

        for (int i = 0; i < PickupIconCount; ++i) {
            loadPickupIcon(i, filenames[i]);
        }
    }

    void loadPickupIcon(int iconIndex, const std::string& filename) {
        const std::vector<std::string> paths = {
            "assets/pickups/" + filename,
            "../assets/pickups/" + filename
        };

        sf::Image iconImage;
        for (const auto& path : paths) {
            if (iconImage.loadFromFile(path)) {
                maskLightCheckerboard(iconImage);
                if (pickupTextures_[iconIndex].loadFromImage(iconImage)) {
                    pickupTextures_[iconIndex].setSmooth(true);
                    hasPickupIcons_[iconIndex] = true;
                }
                return;
            }
        }
    }

    void loadEnemySprite() {
        const std::vector<std::string> paths = {
            "assets/enemy.png",
            "../assets/enemy.png"
        };

        sf::Image enemyImage;
        for (const auto& path : paths) {
            if (enemyImage.loadFromFile(path)) {
                const sf::Vector2u imageSize = enemyImage.getSize();
                enemyFrameSize_ = {
                    static_cast<int>(imageSize.x / EnemySpriteColumns),
                    static_cast<int>(imageSize.y / EnemySpriteRows)
                };

                maskLightCheckerboard(enemyImage);
                enemyFrameReferenceHeight_ = 1;
                for (int frame = 0; frame < EnemySpriteFrameCount; ++frame) {
                    const int frameX = frame % EnemySpriteColumns;
                    const int frameY = frame / EnemySpriteColumns;
                    const sf::IntRect cell{
                        frameX * enemyFrameSize_.x,
                        frameY * enemyFrameSize_.y,
                        enemyFrameSize_.x,
                        enemyFrameSize_.y
                    };
                    enemyFrameRects_[frame] = visibleBounds(enemyImage, cell);
                    enemyFrameReferenceHeight_ = std::max(enemyFrameReferenceHeight_, enemyFrameRects_[frame].height);
                }

                if (enemyTexture_.loadFromImage(enemyImage)) {
                    enemyTexture_.setSmooth(true);
                    hasEnemySprite_ = true;
                }
                return;
            }
        }
    }

    void startBackgroundMusic() {
        const std::vector<std::string> paths = {
            "assets/music/SurvivalArena.mp3",
            "../assets/music/SurvivalArena.mp3"
        };

        for (const auto& path : paths) {
            if (backgroundMusic_.openFromFile(path)) {
                backgroundMusic_.setLoop(true);
                backgroundMusic_.setVolume(45.0f);
                backgroundMusic_.play();
                return;
            }
        }
    }

    int pickupIconIndex(const Pickup& pickup) const {
        switch (pickup.type) {
            case PickupType::Ammo:
                return 0;
            case PickupType::Health:
                return 1;
            case PickupType::Weapon:
                return 2 + clampWeaponIndex(pickup.weaponIndex);
        }

        return 0;
    }

    int clampWeaponIndex(int weaponIndex) const {
        return std::max(0, std::min(weaponIndex, PlayerWeaponSpriteCount - 1));
    }

    void updatePlayerAnimation(sf::Vector2f movementDirection, float dt) {
        if (length(movementDirection) > 0.0f) {
            playerAnimationTime_ += dt;
            playerFrame_ = static_cast<int>(playerAnimationTime_ * 10.0f) % (PlayerSpriteColumns * PlayerSpriteRows);
            return;
        }

        playerAnimationTime_ = 0.0f;
        playerFrame_ = 0;
    }

    void drawArena() {
        sf::RectangleShape floor({WindowWidth - ArenaPadding * 2.0f, WindowHeight - ArenaPadding * 2.0f});
        floor.setPosition(ArenaPadding, ArenaPadding);
        floor.setFillColor(sf::Color::Transparent);
        floor.setOutlineColor(sf::Color(115, 128, 145, 180));
        floor.setOutlineThickness(2.0f);
        window_.draw(floor);

        sf::VertexArray grid(sf::Lines);
        const sf::Color gridColor(30, 36, 45, 70);
        for (float x = ArenaPadding + 64.0f; x < WindowWidth - ArenaPadding; x += 64.0f) {
            grid.append(sf::Vertex({x, ArenaPadding}, gridColor));
            grid.append(sf::Vertex({x, WindowHeight - ArenaPadding}, gridColor));
        }
        for (float y = ArenaPadding + 64.0f; y < WindowHeight - ArenaPadding; y += 64.0f) {
            grid.append(sf::Vertex({ArenaPadding, y}, gridColor));
            grid.append(sf::Vertex({WindowWidth - ArenaPadding, y}, gridColor));
        }
        window_.draw(grid);
    }

    void drawPlayer() {
        const int weaponSpriteIndex = player_.weaponIndex;
        const bool hasCurrentWeaponSprite = weaponSpriteIndex >= 0
            && weaponSpriteIndex < PlayerWeaponSpriteCount
            && hasPlayerSprites_[weaponSpriteIndex];
        const int spriteIndex = hasCurrentWeaponSprite ? weaponSpriteIndex : 0;

        if (hasPlayerSprites_[spriteIndex]) {
            const int frameX = playerFrame_ % PlayerSpriteColumns;
            const int frameY = playerFrame_ / PlayerSpriteColumns;
            sf::Sprite sprite(playerTextures_[spriteIndex]);
            sprite.setTextureRect({
                frameX * playerFrameSize_.x,
                frameY * playerFrameSize_.y,
                playerFrameSize_.x,
                playerFrameSize_.y
            });
            sprite.setOrigin(playerFrameSize_.x * 0.5f, playerFrameSize_.y * 0.5f);

            const float scale = PlayerSpriteHeight / static_cast<float>(playerFrameSize_.y);
            sprite.setScale(scale, scale);
            sprite.setPosition(player_.position);
            sprite.setRotation(angleDegrees(player_.aim) - 90.0f);
            sprite.setColor(player_.invulnerable > 0.0f ? sf::Color(170, 230, 255, 210) : sf::Color::White);
            window_.draw(sprite);
            return;
        }

        sf::CircleShape body(PlayerRadius);
        body.setOrigin(PlayerRadius, PlayerRadius);
        body.setPosition(player_.position);
        body.setFillColor(player_.invulnerable > 0.0f ? sf::Color(150, 230, 255) : sf::Color(75, 190, 145));
        body.setOutlineColor(sf::Color(220, 255, 235));
        body.setOutlineThickness(2.0f);
        window_.draw(body);

        sf::RectangleShape barrel({34.0f, 8.0f});
        barrel.setOrigin(4.0f, 4.0f);
        barrel.setPosition(player_.position);
        barrel.setRotation(angleDegrees(player_.aim));
        barrel.setFillColor(sf::Color(210, 218, 225));
        window_.draw(barrel);
    }

    void drawEnemies() {
        for (const auto& enemy : enemies_) {
            if (hasEnemySprite_) {
                const int frame = static_cast<int>(enemy.animationTime * 10.0f) % EnemySpriteFrameCount;
                const sf::IntRect frameRect = enemyFrameRects_[frame];

                sf::Sprite sprite(enemyTexture_);
                sprite.setTextureRect(frameRect);
                sprite.setOrigin(frameRect.width * 0.5f, frameRect.height * 0.5f);

                const float scale = EnemySpriteHeight / static_cast<float>(enemyFrameReferenceHeight_);
                sprite.setScale(scale, scale);
                sprite.setPosition(enemy.position);
                sprite.setRotation(angleDegrees(enemy.velocity) - 90.0f);
                window_.draw(sprite);
            } else {
                sf::CircleShape shape(EnemyRadius);
                shape.setOrigin(EnemyRadius, EnemyRadius);
                shape.setPosition(enemy.position);
                shape.setFillColor(sf::Color(210, 65, 83));
                shape.setOutlineColor(sf::Color(255, 165, 165));
                shape.setOutlineThickness(1.5f);
                window_.draw(shape);
            }

            if (enemy.maxHealth > 1) {
                sf::RectangleShape bg({EnemyRadius * 2.0f, 4.0f});
                bg.setOrigin(EnemyRadius, 14.0f);
                bg.setPosition(enemy.position);
                bg.setFillColor(sf::Color(70, 30, 35));
                window_.draw(bg);

                sf::RectangleShape hp({EnemyRadius * 2.0f * static_cast<float>(enemy.health) / enemy.maxHealth, 4.0f});
                hp.setOrigin(EnemyRadius, 14.0f);
                hp.setPosition(enemy.position);
                hp.setFillColor(sf::Color(255, 120, 130));
                window_.draw(hp);
            }
        }
    }

    void drawBullets() {
        for (const auto& bullet : bullets_) {
            sf::CircleShape shape(5.0f);
            shape.setOrigin(5.0f, 5.0f);
            shape.setPosition(bullet.position);
            shape.setFillColor(bullet.color);
            window_.draw(shape);
        }
    }

    void drawPickups() {
        for (const auto& pickup : pickups_) {
            const float radius = PickupRadius + std::sin(pickup.pulse) * 1.5f;

            const int iconIndex = pickupIconIndex(pickup);
            if (iconIndex >= 0 && iconIndex < PickupIconCount && hasPickupIcons_[iconIndex]) {
                sf::CircleShape glow(radius + 6.0f, 32);
                glow.setOrigin(radius + 6.0f, radius + 6.0f);
                glow.setPosition(pickup.position);
                glow.setFillColor(sf::Color(18, 24, 30, 130));
                glow.setOutlineColor(sf::Color(120, 210, 255, 120));
                glow.setOutlineThickness(1.0f);
                window_.draw(glow);

                sf::Sprite icon(pickupTextures_[iconIndex]);
                const sf::Vector2u textureSize = pickupTextures_[iconIndex].getSize();
                icon.setOrigin(textureSize.x * 0.5f, textureSize.y * 0.5f);

                const float pulseScale = PickupIconSize + std::sin(pickup.pulse) * 3.0f;
                const float scale = pulseScale / static_cast<float>(std::max(textureSize.x, textureSize.y));
                icon.setScale(scale, scale);
                icon.setPosition(pickup.position);
                window_.draw(icon);
                continue;
            }

            sf::CircleShape shape(radius, pickup.type == PickupType::Weapon ? 6 : 24);
            shape.setOrigin(radius, radius);
            shape.setPosition(pickup.position);

            if (pickup.type == PickupType::Ammo) {
                shape.setFillColor(sf::Color(240, 205, 75));
            } else if (pickup.type == PickupType::Health) {
                shape.setFillColor(sf::Color(90, 220, 110));
            } else {
                shape.setFillColor(Weapons[pickup.weaponIndex].color);
            }
            shape.setOutlineColor(sf::Color(245, 245, 245));
            shape.setOutlineThickness(1.5f);
            window_.draw(shape);
        }
    }

    void drawHud() {
        sf::RectangleShape healthBg({220.0f, 18.0f});
        healthBg.setPosition(24.0f, 18.0f);
        healthBg.setFillColor(sf::Color(60, 28, 34));
        window_.draw(healthBg);

        sf::RectangleShape healthBar({220.0f * static_cast<float>(player_.health) / 100.0f, 18.0f});
        healthBar.setPosition(24.0f, 18.0f);
        healthBar.setFillColor(sf::Color(82, 215, 116));
        window_.draw(healthBar);

        std::ostringstream hud;
        hud << "Health " << player_.health
            << "   Ammo " << player_.ammo
            << "   Weapon " << Weapons[player_.weaponIndex].name
            << "   Wave " << wave_
            << "   Score " << score_
            << "   Time " << static_cast<int>(elapsed_) << "s";
        drawText(hud.str(), {26.0f, 42.0f}, 20, sf::Color(232, 236, 242));

        drawText("WASD move   Mouse aim   Click/Space shoot   P pause", {WindowWidth - 520.0f, WindowHeight - 32.0f}, 16, sf::Color(165, 174, 188));
    }

    void drawCenterText(const std::string& title, const std::string& subtitle) {
        sf::RectangleShape overlay({static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)});
        overlay.setFillColor(sf::Color(0, 0, 0, 135));
        window_.draw(overlay);

        drawText(title, {0.0f, WindowHeight * 0.42f}, 54, sf::Color::White, true);
        drawText(subtitle, {0.0f, WindowHeight * 0.52f}, 24, sf::Color(220, 225, 235), true);
    }

    void drawText(const std::string& text, sf::Vector2f position, unsigned size, sf::Color color, bool centered = false) {
        sf::Text label;
        label.setFont(font_);
        label.setString(text);
        label.setCharacterSize(size);
        label.setFillColor(color);
        if (centered) {
            const sf::FloatRect bounds = label.getLocalBounds();
            label.setOrigin(bounds.left + bounds.width * 0.5f, bounds.top + bounds.height * 0.5f);
            label.setPosition(WindowWidth * 0.5f, position.y);
        } else {
            label.setPosition(position);
        }
        window_.draw(label);
    }

    sf::RenderWindow window_;
    sf::Font font_;
    sf::Texture backgroundTexture_;
    sf::Sprite backgroundSprite_;
    std::array<sf::Texture, PlayerWeaponSpriteCount> playerTextures_;
    sf::Vector2i playerFrameSize_{0, 0};
    std::array<sf::Texture, PickupIconCount> pickupTextures_;
    sf::Texture enemyTexture_;
    sf::Vector2i enemyFrameSize_{0, 0};
    std::array<sf::IntRect, EnemySpriteFrameCount> enemyFrameRects_;
    int enemyFrameReferenceHeight_ = 1;
    sf::Music backgroundMusic_;
    std::mt19937 rng_;
    Player player_;
    std::vector<Bullet> bullets_;
    std::vector<Enemy> enemies_;
    std::vector<Pickup> pickups_;
    float elapsed_ = 0.0f;
    int score_ = 0;
    int wave_ = 0;
    int enemiesToSpawn_ = 0;
    float spawnTimer_ = 0.0f;
    float pickupTimer_ = 0.0f;
    float waveBreak_ = 0.0f;
    float playerAnimationTime_ = 0.0f;
    int playerFrame_ = 0;
    bool paused_ = false;
    bool gameOver_ = false;
    bool hasBackground_ = false;
    std::array<bool, PlayerWeaponSpriteCount> hasPlayerSprites_{false, false, false};
    std::array<bool, PickupIconCount> hasPickupIcons_{false, false, false, false, false};
    bool hasEnemySprite_ = false;
};

} // namespace

int main() {
    Game game;
    game.run();
    return EXIT_SUCCESS;
}

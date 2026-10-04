// net_demo.cpp
// Сборка:  g++ -std=c++20 -O2 -o net_demo net_demo.cpp   (MSVC: /std:c++20)
// Запуск просто запускаете — смотрите как меняются пакеты от тика к тику.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <thread>
#include <vector>

// ============================ НАСТРОЙКИ ============================
constexpr int    TICKS      = 26;     // сколько тиков показываем
constexpr float  DT         = 0.10f;  // длина тика, 10 Гц (вместо 60 — чтобы глазом видеть)
constexpr float  AOI_RADIUS = 25.0f;  // радиус «пузыря интереса»
constexpr int    POS_SCALE  = 10;     // квантизация позиции: 1 шаг = 0.1 единицы
constexpr int8_t MAX_DELTA  = 100;    // макс. дельта по оси за тик (влезает в int8)

// ---- биты маски «что изменилось» (наш протокол) ----
enum : uint8_t {
    F_SPAWN = 1 << 0,  // первая отправка: абсолютные значения
    F_DX    = 1 << 1,  // изменился X — шлём дельту
    F_DY    = 1 << 2,  // изменился Y — шлём дельту
    F_HP    = 1 << 3,  // изменился HP
};

static std::string maskStr(uint8_t m) {
    std::string s;
    auto add = [&](const char* n) { if (!s.empty()) s += '|'; s += n; };
    if (m & F_SPAWN) add("SPAWN");
    if (m & F_DX)    add("DX");
    if (m & F_DY)    add("DY");
    if (m & F_HP)    add("HP");
    return s;
}

// ============================ МИР ============================
struct Entity {
    int id; std::string name; char icon;      // icon — буква на мини-карте
    float x = 0, y = 0, vx = 0, vy = 0;
    int hp = 100;
    bool alive = true;
};

// Что сервер УЖЕ отправил этому клиенту (в реальной игре — своя копия на клиента)
struct LastSent { bool spawned = false; int qx = 0, qy = 0, hp = -1; };

// Одна запись в пакете
struct Entry {
    int id = 0; std::string name;
    uint8_t mask = 0;
    int dx = 0, dy = 0;          // квантованные дельты
    int qx = 0, qy = 0, hp = 0;  // абсолюты (при SPAWN) + для лога
    bool skipped = false;        // был в пузыре, но без изменений -> 0 байт
    float srvX = 0, srvY = 0;    // «истина» сервера, для показа расхождения
};

struct Packet {
    std::vector<uint8_t> bytes;  // собственно то, что «ушло в сеть»
    std::vector<Entry>   log;    // человекочитаемая расшифровка
    int    inBubble   = 0;       // сколько сущностей попало в пузырь
    size_t naiveBytes = 0;       // сколько это стоило бы «наивно» (12 Б/сущность)
};

// ================== СЕРИАЛИЗАЦИЯ (сервер -> байты) ==================
static void pushU16(std::vector<uint8_t>& b, int v) {
    b.push_back(uint8_t(v & 0xFF));
    b.push_back(uint8_t((v >> 8) & 0xFF));
}

static Packet buildPacket(const std::vector<Entity>& ents, const Entity& cl,
                          std::map<int, LastSent>& last)
{
    Packet p;
    p.bytes.push_back(0);            // [0] = количество записей, заполним в конце
    int count = 0;

    for (const auto& e : ents) {
        if (!e.alive) continue;

        // ---- ПУЗЫРЬ ИНТЕРЕСА: вне радиуса не отправляем ВООБЩЕ ----
        float dist = std::hypot(e.x - cl.x, e.y - cl.y);
        if (dist > AOI_RADIUS) continue;

        ++p.inBubble;
        p.naiveBytes += 12;          // наивный конкурент: 2 float поз + 1 float hp

        Entry en; en.id = e.id; en.name = e.name; en.srvX = e.x; en.srvY = e.y;
        LastSent& ls = last[e.id];

        // ---- КВАНТИЗАЦИЯ: float -> целое на сетке 0.1 ----
        int qx = (int)std::lround(e.x * POS_SCALE);
        int qy = (int)std::lround(e.y * POS_SCALE);

        if (!ls.spawned) {
            // Первая отправка: абсолютные значения
            en.mask = F_SPAWN | F_DX | F_DY | F_HP;
            en.qx = qx; en.qy = qy; en.hp = e.hp;
            p.bytes.push_back(uint8_t(e.id));
            p.bytes.push_back(en.mask);
            pushU16(p.bytes, qx);
            pushU16(p.bytes, qy);
            pushU16(p.bytes, e.hp);              // итого 1+1+2+2+2 = 8 байт
            ls = {true, qx, qy, e.hp};
        } else {
            // ---- ДЕЛЬТЫ: сравниваем с тем, что клиент уже имеет ----
            if (qx != ls.qx)     en.mask |= F_DX;
            if (qy != ls.qy)     en.mask |= F_DY;
            if (e.hp != ls.hp)   en.mask |= F_HP;

            if (en.mask == 0) {                  // ничего не изменилось!
                en.skipped = true;
                p.log.push_back(en);
                continue;                        // НОЛЬ байт в пакет
            }

            en.dx = std::clamp(qx - ls.qx, -(int)MAX_DELTA, (int)MAX_DELTA);
            en.dy = std::clamp(qy - ls.qy, -(int)MAX_DELTA, (int)MAX_DELTA);
            en.hp = e.hp;

            p.bytes.push_back(uint8_t(e.id));
            p.bytes.push_back(en.mask);
            if (en.mask & F_DX) { p.bytes.push_back(uint8_t(int8_t(en.dx))); ls.qx += en.dx; }
            if (en.mask & F_DY) { p.bytes.push_back(uint8_t(int8_t(en.dy))); ls.qy += en.dy; }
            if (en.mask & F_HP) { pushU16(p.bytes, en.hp);                   ls.hp  = en.hp; }
        }
        ++count;
        p.log.push_back(en);
    }
    p.bytes[0] = uint8_t(count);
    return p;
}

// ================== КЛИЕНТ: разбор пакета ==================
struct View { bool spawned = false; int qx = 0, qy = 0; float x = 0, y = 0; int hp = 100; };

static void applyPacket(const Packet& p, std::map<int, View>& view) {
    for (const auto& en : p.log) {
        if (en.skipped) continue;                // данных нет — оставляем старое состояние
        View& v = view[en.id];
        if (en.mask & F_SPAWN) {                 // абсолюты
            v.spawned = true;
            v.qx = en.qx; v.qy = en.qy; v.hp = en.hp;
        } else {                                 // применяем дельты
            if (en.mask & F_DX) v.qx += en.dx;
            if (en.mask & F_DY) v.qy += en.dy;
            if (en.mask & F_HP) v.hp  = en.hp;
        }
        v.x = float(v.qx) / POS_SCALE;           // деквантизация обратно в float
        v.y = float(v.qy) / POS_SCALE;
    }
}

// ================== МИНИ-КАРТА ДЛЯ НАГЛЯДНОСТИ ==================
static void drawMap(const Entity& cl, const std::vector<Entity>& ents) {
    const int W = 60, H = 12;
    std::vector<std::string> g(H, std::string(W, '.'));
    auto put = [&](float x, float y, char c) {
        int cx = (int)std::lround(x), cy = (int)std::lround(y);
        if (cx >= 0 && cx < W && cy >= 0 && cy < H) g[cy][cx] = c;
    };
    for (const auto& e : ents) if (e.alive) put(e.x, e.y, e.icon);
    put(cl.x, cl.y, 'C');
    printf("      0    5    10   15   20   25   30   35   40   45   50   55   x\n");
    for (int r = H - 1; r >= 0; --r) printf("   %2d |%s\n", r, g[r].c_str());
}

// ================== ГЛАВНЫЙ ЦИКЛ ==================
int main() {
    std::vector<Entity> ents = {
        {1, "Враг",    'E', 40, 6, -10, 0, 100, true},  // идёт к игроку, потом остановится
        {2, "Ящик",    'K', 15, 3,   0, 0, 100, true},  // стоит — после спавна шлётся 0 байт
        {3, "Снаряд",  'S',  0, 0,  25, 0, 100, false}, // заспавнится на тике 8
        {4, "Обломки", 'D', 55, 10,  0, 0, 100, true},  // далеко — НИКОГДА не отправится
    };
    Entity cl {0, "Клиент", 'C', 5, 6, 2, 0, 100, true}; // сам игрок, медленно идёт вправо

    std::map<int, LastSent> lastSent;  // «память сервера»: что уже отправлено клиенту
    std::map<int, View>     view;      // состояние мира у клиента

    size_t totalOpt = 0, totalNaiveBubble = 0, totalNaiveAll = 0;

    for (int tick = 1; tick <= TICKS; ++tick) {
        // ---------- 1. Симуляция мира на сервере ----------
        cl.x += cl.vx * DT;
        for (auto& e : ents) if (e.alive) { e.x += e.vx * DT; e.y += e.vy * DT; }

        if (tick == 8)  { ents[2].alive = true; ents[2].x = cl.x + 1; ents[2].y = cl.y; }
        if (tick == 14) ents[0].hp = 70;                       // врагу прилетело
        if (ents[0].x <= 20) { ents[0].x = 20; ents[0].vx = 0; } // враг остановился

        // ---------- 2. Сервер формирует пакет ----------
        Packet p = buildPacket(ents, cl, lastSent);

        // ---------- 3. «Сеть»: клиент получает и применяет ----------
        applyPacket(p, view);

        // ---------- 4. Печать ----------
        int aliveCount = 0; for (auto& e : ents) if (e.alive) ++aliveCount;
        totalOpt         += p.bytes.size();
        totalNaiveBubble += p.naiveBytes;
        totalNaiveAll    += size_t(aliveCount) * 12;

        printf("\n================ ТИК %2d ================\n", tick);
        drawMap(cl, ents);

        printf("Сервер (истина):\n");
        for (const auto& e : ents) {
            if (!e.alive) continue;
            float d = std::hypot(e.x - cl.x, e.y - cl.y);
            printf("   id=%d %-7s (%6.2f,%5.2f) hp=%3d  дист=%5.1f  %s\n",
                   e.id, e.name.c_str(), e.x, e.y, e.hp, d,
                   d <= AOI_RADIUS ? "[в пузыре]" : "[вне пузыря — не шлём]");
        }

        printf("Пакет: %zu байт | в пузыре: %d сущн. | наивно было бы: %zu байт\n",
               p.bytes.size(), p.inBubble, p.naiveBytes);
        printf("  сырые байты: ");
        for (uint8_t b : p.bytes) printf("%02X ", b);
        printf("\n");
        for (const auto& en : p.log) {
            if (en.skipped) {
                printf("  id=%d %-7s без изменений -> пропущен (0 байт)\n", en.id, en.name.c_str());
            } else {
                std::string ms = maskStr(en.mask);
                printf("  id=%d %-7s [%s] ", en.id, en.name.c_str(), ms.c_str());
                if (en.mask & F_SPAWN) printf("qx=%d qy=%d hp=%d", en.qx, en.qy, en.hp);
                else {
                    if (en.mask & F_DX) printf("dx=%+d ", en.dx);
                    if (en.mask & F_DY) printf("dy=%+d ", en.dy);
                    if (en.mask & F_HP) printf("hp=%d ", en.hp);
                }
                printf("\n");
            }
        }

        printf("Клиент после применения:\n");
        for (const auto& en : p.log) {
            const View& v = view[en.id];
            if (!v.spawned) continue;
            float err = std::hypot(v.x - en.srvX, v.y - en.srvY);
            printf("   id=%d %-7s поз=(%6.2f,%6.2f) hp=%3d | расхождение: %.2f%s\n",
                   en.id, en.name.c_str(), v.x, v.y, v.hp, err,
                   en.skipped ? " (данных не было, состояние прежнее)" : "");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    printf("\n=============== ИТОГ ЗА %d ТИКОВ ===============\n", TICKS);
    printf("  Наивно (все сущности, float32, каждый тик): %6zu байт\n", totalNaiveAll);
    printf("  + пузырь интереса (AoI)                  : %6zu байт\n", totalNaiveBubble);
    printf("  + дельты + квантизация (наш протокол)    : %6zu байт\n", totalOpt);
    printf("  Выигрыш: x%.1f\n", float(totalNaiveAll) / float(totalOpt));
}
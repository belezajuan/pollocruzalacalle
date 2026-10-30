// ============================================================
//  EL POLLO CRUZA LA CALLE - version grafica (raylib 6.x)
//  Sin audio, para que nunca se trabe en ningun equipo.
//
//  Controles:
//    W/A/S/D o flechas = mover (mantener la tecla = avanzar seguido)
//    ESPACIO o J       = golpear un auto cercano (usa una carga)
//    P = pausa        Q = volver al menu
// ============================================================
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <string>
#include <vector>

// ------------------------- Constantes -------------------------
const int ANCHO = 20;                                // columnas
const int CARRILES = 6;                              // calles con autos
const int FILAS = CARRILES + 2;                      // fila 0 = meta, ultima = inicio
const int NIVEL_FINAL = 5;
const int CELDA = 48;                                // tamano de cada celda (px)
const int MARGEN_Y = 90;                             // alto del marcador de arriba
const int VENTANA_W = ANCHO * CELDA;                 // 960
const int VENTANA_H = MARGEN_Y + FILAS * CELDA + 70; // 544
const int PUNTOS_AVANCE = 10;                        // puntos por cada fila nueva
const int PUNTOS_GOLPE = 25;                         // puntos por destruir un auto
const int MAX_CARGAS = 5;                            // golpes que puedes guardar
const float RECARGA_SEG = 8.0f;                      // segundos para recuperar un golpe
const float INVUL_TIEMPO = 1.5f;                     // segundos protegido al reaparecer

enum Estado { MENU, INSTR, CUENTA, JUEGO, PAUSA, MUERTE, NIVEL, BANNER, GAMEOVER, VICTORIA };

static const Color COLORES_AUTO[7] = {
    {220, 60, 60, 255},   {60, 120, 230, 255}, {240, 190, 40, 255}, {70, 180, 90, 255},
    {170, 90, 210, 255},  {240, 240, 240, 255}, {50, 50, 60, 255}};

static const char *NOMBRES_NIVEL[NIVEL_FINAL] = {"FACIL", "NORMAL", "MEDIO", "DIFICIL", "EXTREMO"};

// ------------------------- Utilidades de dibujo -------------------------
static void tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

static Color mezclar(Color a, Color b, float k) {
    return Color{(unsigned char)(a.r + (b.r - a.r) * k), (unsigned char)(a.g + (b.g - a.g) * k),
                 (unsigned char)(a.b + (b.b - a.b) * k), 255};
}

static void textoCentrado(const char *t, int y, int tam, Color c, int cx = VENTANA_W / 2) {
    int w = MeasureText(t, tam);
    DrawText(t, cx - w / 2 + 2, y + 2, tam, Color{0, 0, 0, 180});
    DrawText(t, cx - w / 2, y, tam, c);
}

// Titulo con letras que ondulan
static void tituloOnda(const std::string &s, int y, int tam, float t, Color c) {
    int total = MeasureText(s.c_str(), tam);
    int x = VENTANA_W / 2 - total / 2;
    for (size_t i = 0; i < s.size(); i++) {
        std::string ch(1, s[i]);
        int dy = (int)(std::sin(t * 3.0f + i * 0.6f) * 8.0f);
        DrawText(ch.c_str(), x + 3, y + dy + 4, tam, Color{0, 0, 0, 170});
        DrawText(ch.c_str(), x, y + dy, tam, c);
        x += MeasureText(ch.c_str(), tam);
    }
}

// Nubes que se desplazan
static void dibujarNubes(float t, float alturaBase, float alpha) {
    for (int i = 0; i < 4; i++) {
        float velocidad = 12.0f + i * 5.0f;
        float x = std::fmod(t * velocidad + i * 260.0f, (float)VENTANA_W + 240.0f) - 120.0f;
        float y = alturaBase + (i % 2) * 22.0f;
        Color c = Fade(WHITE, alpha);
        DrawEllipse((int)x, (int)y, 46.0f, 16.0f, c);
        DrawEllipse((int)(x + 36.0f), (int)(y - 8.0f), 32.0f, 16.0f, c);
        DrawEllipse((int)(x - 30.0f), (int)(y - 2.0f), 28.0f, 12.0f, c);
    }
}

static void dibujarCorazon(float x, float y, bool lleno) {
    Color c = lleno ? Color{230, 50, 70, 255} : Color{70, 70, 90, 255};
    DrawCircle((int)(x - 6), (int)y, 7.0f, c);
    DrawCircle((int)(x + 6), (int)y, 7.0f, c);
    tri({x - 12.5f, y + 3}, {x, y + 18}, {x + 12.5f, y + 3}, c);
    if (lleno) DrawCircle((int)(x - 8), (int)(y - 2), 2.0f, Color{255, 200, 210, 255});
}

// Rueda con rayos que giran
static void dibujarRueda(float cx, float cy, float r, float ang) {
    DrawCircle((int)cx, (int)cy, r, Color{20, 20, 24, 255});
    DrawCircle((int)cx, (int)cy, r * 0.45f, Color{170, 170, 180, 255});
    for (int k = 0; k < 2; k++) {
        float a = ang + k * PI / 2.0f;
        DrawLineEx({cx, cy}, {cx + std::cos(a) * r * 0.8f, cy + std::sin(a) * r * 0.8f}, 1.5f,
                   Color{110, 110, 120, 255});
    }
}

// Auto visto de lado. x,y = esquina superior izquierda.
// dir = hacia donde va (1 derecha, -1 izquierda). camion = cabina corta y caja larga.
// ang = angulo de giro de las ruedas.
static void dibujarCoche(float x, float y, float w, float h, Color c, int dir, bool camion, float ang) {
    // sombra
    DrawEllipse((int)(x + w / 2), (int)(y + h + 3), w * 0.46f, 4.0f, Color{0, 0, 0, 90});
    // carroceria
    DrawRectangleRounded(Rectangle{x, y + h * 0.28f, w, h * 0.72f}, 0.28f, 6, c);
    // cabina
    float cabX = camion ? x + w * 0.56f : x + w * 0.2f;
    float cabW = camion ? w * 0.4f : w * 0.58f;
    DrawRectangleRounded(Rectangle{cabX, y + 2.0f, cabW, h * 0.62f}, 0.35f, 6, c);
    // reflejo
    DrawRectangleRounded(Rectangle{x + 5.0f, y + h * 0.32f, w - 10.0f, 4.0f}, 0.5f, 4, Fade(WHITE, 0.22f));
    // ventanas
    Color vidrio{170, 215, 245, 255};
    DrawRectangleRounded(Rectangle{cabX + 4.0f, y + 6.0f, cabW - 8.0f, h * 0.36f}, 0.3f, 4, vidrio);
    // division de puerta
    DrawRectangle((int)(x + w * 0.5f), (int)(y + h * 0.5f), 2, (int)(h * 0.4f), Fade(BLACK, 0.35f));
    // luces: delante amarillas con brillo, detras rojas
    float frontX = dir > 0 ? x + w - 4.0f : x + 4.0f;
    float backX = dir > 0 ? x + 2.0f : x + w - 6.0f;
    Color amarillo{255, 245, 170, 255};
    // haz de luz suave hacia donde va el auto
    float beamX = dir > 0 ? x + w : x;
    float beamD = dir > 0 ? 70.0f : -70.0f;
    tri({beamX, y + h * 0.35f}, {beamX + beamD, y - 10.0f}, {beamX + beamD, y + h + 8.0f}, Fade(amarillo, 0.10f));
    DrawCircleGradient(Vector2{frontX, y + h * 0.6f}, 10.0f, Fade(amarillo, 0.55f), Fade(amarillo, 0.0f));
    DrawRectangle((int)frontX - 2, (int)(y + h * 0.5f), 4, 5, Color{255, 250, 200, 255});
    DrawRectangle((int)backX, (int)(y + h * 0.5f), 4, 5, Color{200, 20, 20, 255});
    // ruedas que giran
    dibujarRueda(x + w * 0.22f, y + h - 2.0f, 6.5f, ang);
    dibujarRueda(x + w * 0.78f, y + h - 2.0f, 6.5f, ang);
}

// e = aplastamiento vertical (1 = normal), aleteo = movimiento del ala,
// ojosCerrados = parpadeo, paso = patas al caminar (-1 a 1)
static void dibujarPollo(float cx, float cy, float e = 1.0f, float aleteo = 0.0f,
                         bool ojosCerrados = false, float paso = 0.0f) {
    float base = cy + 16;
    auto Y = [&](float dy) { return base + (dy - 16) * e; };
    auto V = [&](float r) { return std::max(1.0f, r * e); };
    Color naranja{255, 160, 0, 255};
    DrawEllipse((int)cx, (int)base, 17, 5, Color{0, 0, 0, 70});
    // patas que se mueven al caminar
    DrawLineEx({cx - 6, Y(10)}, {cx - 6 + paso * 3.0f, Y(16)}, 3, naranja);
    DrawLineEx({cx + 6, Y(10)}, {cx + 6 - paso * 3.0f, Y(16)}, 3, naranja);
    DrawEllipse((int)cx, (int)Y(2), 16, V(15), Color{200, 200, 200, 255});
    DrawEllipse((int)cx, (int)Y(2), 15, V(14), WHITE);
    DrawEllipse((int)(cx - 7), (int)Y(4 + aleteo), 7, V(6), Color{225, 225, 225, 255});
    DrawEllipse((int)cx, (int)Y(-12), 9, V(9), WHITE);
    DrawEllipse((int)(cx - 3), (int)Y(-22), 3.5f, V(4), RED);
    DrawEllipse((int)(cx + 3), (int)Y(-22), 3.5f, V(4), RED);   // cresta
    tri({cx - 4, Y(-10)}, {cx + 4, Y(-10)}, {cx, Y(-4)}, naranja);   // pico
    DrawEllipse((int)cx, (int)Y(-2), 2, V(3), RED);            // papada
    if (ojosCerrados) {
        DrawLineEx({cx - 7, Y(-14)}, {cx - 2, Y(-14)}, 2.0f, BLACK);
        DrawLineEx({cx + 2, Y(-14)}, {cx + 7, Y(-14)}, 2.0f, BLACK);
    } else {
        DrawEllipse((int)(cx - 4), (int)Y(-14), 2, V(2), BLACK);
        DrawEllipse((int)(cx + 4), (int)Y(-14), 2, V(2), BLACK);
    }
}

static void dibujarTumba(float cx, float cy) {
    DrawEllipse((int)cx, (int)(cy + 18), 20, 5, Color{0, 0, 0, 80});
    DrawRectangle((int)(cx - 14), (int)(cy - 6), 28, 24, Color{150, 150, 160, 255});
    DrawCircle((int)cx, (int)(cy - 6), 14, Color{150, 150, 160, 255});
    DrawText("RIP", (int)(cx - 11), (int)(cy - 6), 14, Color{50, 50, 60, 255});
}

// ------------------------- Record -------------------------
static int cargarRecord() { std::ifstream f("record.txt"); int r = 0; if (f) f >> r; return r; }
static void guardarRecord(int r) { std::ofstream f("record.txt"); if (f) f << r; }

// ------------------------- Juego (logica) -------------------------
struct Coche { float x, len; bool camion; Color color; };
struct Carril { std::vector<Coche> autos; int dir = 1; float velocidad = 2.0f; };
struct Particula { float x, y, vx, vy, vida, vidaMax, tam; Color c; int tipo; };
struct Popup { float x, y, vida; std::string txt; Color c; };
// Auto destruido que vuela (tipo 0) o se aplasta (tipo 1)
struct Volador { float x, y, vx, vy, vida, vidaMax, w, h; Color c; bool camion; int dir; int tipo; };

struct Juego {
    std::vector<Carril> carriles;
    std::vector<Particula> part;
    std::vector<Popup> popups;
    std::vector<Volador> voladores;
    int px = ANCHO / 2, py = FILAS - 1;
    int vidas = 3, nivel = 1, puntos = 0;
    int cargas = MAX_CARGAS;      // golpes disponibles
    float recarga = 0;            // tiempo acumulado para recuperar un golpe
    float golpeT = 0;             // tiempo de la animacion de golpe
    int filaMin = FILAS - 1;      // fila mas alta alcanzada en este nivel
    float visX = 0, visY = 0;     // posicion suavizada del pollo (px)
    float invul = 0;              // segundos sin poder morir
    std::mt19937 rng{std::random_device{}()};

    float rf(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
    int ri(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }
    float centroX(int x) const { return x * CELDA + CELDA / 2.0f; }
    float centroY(int y) const { return MARGEN_Y + y * CELDA + CELDA / 2.0f; }

    // Dificultad progresiva: nivel 1 = facil (lento, espacioso, sin camiones);
    // nivel 5 = extremo (rapido, juntos, con camiones)
    void generarCarriles() {
        carriles.clear();
        float f = (nivel - 1) / 4.0f;                 // 0 en nivel 1, 1 en nivel 5
        float huecoMin = 4.8f - 2.4f * f;             // espacio libre entre autos (celdas)
        float huecoMax = huecoMin + 1.6f;
        for (int i = 0; i < CARRILES; i++) {
            Carril c;
            c.dir = (i % 2 == 0) ? 1 : -1;
            c.velocidad = 0.9f + 0.5f * nivel + rf(-0.15f, 0.2f);   // celdas por segundo
            float x = rf(-6.0f, 0.0f);
            while (x < ANCHO + 2) {
                Coche a;
                a.camion = (nivel >= 3) && ri(0, 4) == 0;
                a.len = a.camion ? 3.0f : 2.0f;
                a.color = COLORES_AUTO[ri(0, 6)];
                a.x = x;
                c.autos.push_back(a);
                x += a.len + rf(huecoMin, huecoMax);
            }
            carriles.push_back(c);
        }
        filaMin = FILAS - 1;
    }
    void reiniciarPollo() {
        px = ANCHO / 2; py = FILAS - 1;
        visX = centroX(px); visY = centroY(py);
        invul = INVUL_TIEMPO;
    }
    void nuevoJuego() {
        vidas = 3; nivel = 1; puntos = 0;
        cargas = MAX_CARGAS; recarga = 0; golpeT = 0;
        part.clear(); popups.clear(); voladores.clear();
        generarCarriles(); reiniciarPollo();
    }
    void nuevoNivel() {
        nivel++;
        generarCarriles(); reiniciarPollo();
    }

    // true si un auto toca al pollo en su casilla actual
    bool colision() const {
        if (py < 1 || py > CARRILES) return false;
        const Carril &c = carriles[py - 1];
        float x0 = px + 0.2f, x1 = px + 0.8f;
        for (const Coche &a : c.autos)
            if (a.x + a.len > x0 && a.x < x1) return true;
        return false;
    }

    // true si un auto se acerca al pollo y esta a punto de chocarlo
    bool peligro() const {
        if (py < 1 || py > CARRILES) return false;
        const Carril &c = carriles[py - 1];
        for (const Coche &a : c.autos) {
            if (c.dir > 0) {
                float d = px - (a.x + a.len);
                if (d >= 0.0f && d < 1.6f) return true;
            } else {
                float d = a.x - (px + 1.0f);
                if (d >= 0.0f && d < 1.6f) return true;
            }
        }
        return false;
    }

    // Golpea el auto mas cercano de la calle del pollo. Gasta una carga.
    bool golpear() {
        if (cargas <= 0 || py < 1 || py > CARRILES) return false;
        Carril &c = carriles[py - 1];
        int mejor = -1;
        float mejorDist = 1000.0f;
        for (int i = 0; i < (int)c.autos.size(); i++) {
            const Coche &a = c.autos[i];
            if (a.x + a.len > px - 1.5f && a.x < px + 2.5f) {
                float dist = std::fabs(a.x + a.len * 0.5f - (px + 0.5f));
                if (dist < mejorDist) { mejorDist = dist; mejor = i; }
            }
        }
        if (mejor < 0) return false;
        Coche a = c.autos[mejor];
        c.autos.erase(c.autos.begin() + mejor);
        cargas--;
        destruir(a, c.dir);
        golpeT = 0.35f;
        puntos += PUNTOS_GOLPE;
        popups.push_back({centroX(px), centroY(py) - 30.0f, 1.0f,
                          "POW! +" + std::to_string(PUNTOS_GOLPE), Color{255, 230, 80, 255}});
        return true;
    }

    // Animacion aleatoria de destruccion de un auto
    void destruir(const Coche &a, int dir) {
        float h = CELDA - 12.0f;
        float w = a.len * CELDA - 6.0f;
        float x0 = a.x * CELDA + 3.0f;
        float y0 = centroY(py) - h / 2.0f;
        float cx = x0 + w / 2.0f, cy = y0 + h / 2.0f;
        int tipo = ri(0, 3);
        Color fuego1{255, 230, 80, 255}, fuego2{255, 140, 40, 255}, fuego3{230, 60, 40, 255};
        Color humo{120, 120, 128, 255};

        if (tipo == 0) {                      // sale volando hacia arriba
            voladores.push_back({x0, y0, dir * rf(120, 200), -rf(360, 440), 1.6f, 1.6f, w, h,
                                 a.color, a.camion, dir, 0});
            for (int k = 0; k < 6; k++)
                part.push_back({cx + rf(-10, 10), cy + rf(-6, 6), rf(-30, 30), rf(-60, -20), 0.9f, 0.9f,
                                rf(6, 10), Color{160, 160, 170, 255}, 4});
        } else if (tipo == 1) {               // explota
            for (int k = 0; k < 12; k++) {
                Color f = (k % 3 == 0) ? fuego1 : (k % 3 == 1) ? fuego2 : fuego3;
                float vida = rf(0.4f, 0.8f);
                part.push_back({cx + rf(-12, 12), cy + rf(-10, 10), rf(-40, 40), rf(-60, 40), vida, vida,
                                rf(10, 22), f, 3});
            }
            for (int k = 0; k < 14; k++)
                part.push_back({cx, cy, rf(-260, 260), rf(-300, -60), 1.2f, 1.2f, rf(3, 7), a.color, 1});
            for (int k = 0; k < 8; k++)
                part.push_back({cx + rf(-16, 16), cy + rf(-10, 10), rf(-20, 20), rf(-50, -20), 1.0f, 1.0f,
                                rf(8, 14), humo, 4});
        } else if (tipo == 2) {               // se aplasta
            voladores.push_back({x0, y0, 0.0f, 0.0f, 0.7f, 0.7f, w, h, a.color, a.camion, dir, 1});
            for (int k = 0; k < 10; k++)
                part.push_back({cx + rf(-w / 2, w / 2), y0 + h, rf(-120, 120), rf(-200, -60), 0.6f, 0.6f,
                                rf(2, 4), fuego1, 1});
            for (int k = 0; k < 6; k++)
                part.push_back({cx + rf(-w / 2, w / 2), cy, rf(-20, 20), rf(-50, -20), 1.0f, 1.0f,
                                rf(8, 14), humo, 4});
        } else {                              // se parte en pedazos
            for (int k = 0; k < 14; k++) {
                float lado = (k % 2 == 0) ? -1.0f : 1.0f;
                Color pedazo = (k % 3 == 0) ? Color{40, 40, 48, 255} : a.color;
                part.push_back({cx + rf(-w / 2, w / 2), cy + rf(-h / 2, h / 2), lado * rf(80, 220),
                                rf(-260, -80), 1.4f, 1.4f, rf(4, 8), pedazo, 1});
            }
            for (int k = 0; k < 6; k++) {
                float vida = rf(0.3f, 0.6f);
                part.push_back({cx, cy, rf(-120, 120), rf(-120, 120), vida, vida, rf(12, 20), fuego2, 3});
            }
        }
    }

    void moverAutos(float dt) {
        const float L = ANCHO + 6.0f;   // longitud del "anillo" por el que dan la vuelta
        for (Carril &c : carriles)
            for (Coche &a : c.autos) {
                a.x += c.dir * c.velocidad * dt;
                if (c.dir > 0 && a.x > ANCHO + 1.0f) a.x -= L;
                if (c.dir < 0 && a.x + a.len < -1.0f) a.x += L;
            }
    }

    bool mover(int dx, int dy) {
        int nx = px + dx, ny = py + dy;
        if (nx < 0 || nx >= ANCHO || ny < 0 || ny >= FILAS) return false;
        for (int i = 0; i < 4; i++)   // polvo del salto
            part.push_back({visX + rf(-8, 8), visY + 14, rf(-30, 30), rf(-40, -5), 0.35f, 0.35f,
                            rf(2, 4), Color{200, 200, 200, 255}, 0});
        px = nx; py = ny;
        if (py < filaMin) {                       // avanzaste a una fila nueva
            filaMin = py;
            if (py > 0) {
                puntos += PUNTOS_AVANCE;
                popups.push_back({centroX(px), centroY(py) - 20.0f, 1.0f,
                                  "+" + std::to_string(PUNTOS_AVANCE), Color{255, 230, 80, 255}});
            }
        }
        return true;
    }

    // ----- efectos -----
    void plumas() {
        for (int i = 0; i < 22; i++)
            part.push_back({visX, visY, rf(-170, 170), rf(-280, -40), 1.0f, 1.0f, rf(3, 6),
                            (i % 3 == 0) ? Color{230, 50, 50, 255} : WHITE, 2});
    }
    void confeti(int n, float x, float y, bool caer) {
        const Color col[] = {{255, 80, 80, 255}, {80, 220, 120, 255}, {255, 220, 60, 255},
                             {90, 140, 255, 255}, {230, 100, 230, 255}, {80, 220, 230, 255}};
        for (int i = 0; i < n; i++) {
            float vx = caer ? rf(-50, 50) : rf(-160, 160);
            float vy = caer ? rf(80, 180) : rf(-260, -60);
            part.push_back({x + rf(-10, 10), y, vx, vy, 1.8f, 1.8f, rf(3, 6), col[ri(0, 5)], 1});
        }
    }

    // dt = tiempo real; ritmo = 0.25 cuando el tiempo esta lento (solo afecta al juego, no al pollo)
    void actualizarEfectos(float dt, float ritmo) {
        float d = dt * ritmo;
        for (Particula &p : part) {
            p.x += p.vx * d;
            p.y += p.vy * d;
            p.vy += (p.tipo == 1 ? 420.0f : p.tipo == 2 ? 160.0f : 0.0f) * d;
            p.vida -= d;
        }
        part.erase(std::remove_if(part.begin(), part.end(),
                   [](const Particula &p) { return p.vida <= 0; }), part.end());

        for (Volador &v : voladores) {
            v.vida -= d;
            if (v.tipo == 0) {
                v.vy += 700.0f * d;
                v.x += v.vx * d;
                v.y += v.vy * d;
                if (rf(0.0f, 1.0f) < 0.5f)
                    part.push_back({v.x + v.w / 2.0f, v.y + v.h / 2.0f, rf(-20, 20), rf(-50, -20), 0.8f, 0.8f,
                                    rf(5, 9), Color{170, 170, 178, 255}, 4});
            }
        }
        voladores.erase(std::remove_if(voladores.begin(), voladores.end(),
                        [](const Volador &v) {
                            return v.vida <= 0 || v.y > VENTANA_H + 60 || v.x < -200 || v.x > VENTANA_W + 200;
                        }), voladores.end());

        for (Popup &p : popups) { p.y -= 40.0f * d; p.vida -= d; }
        popups.erase(std::remove_if(popups.begin(), popups.end(),
                     [](const Popup &p) { return p.vida <= 0; }), popups.end());

        if (invul > 0) invul -= d;
        if (golpeT > 0) golpeT -= d;
        if (cargas < MAX_CARGAS) {
            recarga += d;
            if (recarga >= RECARGA_SEG) { recarga = 0; cargas++; }
        }

        // el pollo se desliza suavemente hacia su casilla (tiempo real)
        float k = std::min(1.0f, dt * 18.0f);
        visX += (centroX(px) - visX) * k;
        visY += (centroY(py) - visY) * k;
    }
};

// ------------------------- Dibujo de la escena -------------------------
static void dibujarEscena(const Juego &g, Estado est, float te, float t) {
    float oscuro = (g.nivel - 1) / 4.0f;   // la calle se oscurece en niveles altos
    Color asfaltoA = mezclar(Color{74, 77, 86, 255}, Color{36, 38, 56, 255}, oscuro);
    Color asfaltoB = mezclar(Color{56, 59, 67, 255}, Color{24, 26, 40, 255}, oscuro);

    // meta: cesped con bandera de cuadros
    DrawRectangleGradientV(0, MARGEN_Y, VENTANA_W, CELDA, Color{80, 180, 90, 255}, Color{50, 130, 60, 255});
    int mitad = CELDA / 2;
    for (int cx = 0; cx < ANCHO * 2; cx++)
        for (int cy = 0; cy < 2; cy++)
            DrawRectangle(cx * mitad, MARGEN_Y + cy * mitad, mitad, mitad,
                          ((cx + cy) % 2 == 0) ? Color{240, 240, 240, 170} : Color{30, 30, 30, 170});
    DrawRectangle(8, MARGEN_Y + 12, 70, 24, Color{0, 0, 0, 200});
    DrawText("META", 16, MARGEN_Y + 16, 18, Color{255, 230, 80, 255});

    // carriles de asfalto
    for (int i = 0; i < CARRILES; i++) {
        int yTop = MARGEN_Y + (i + 1) * CELDA;
        DrawRectangleGradientV(0, yTop, VENTANA_W, CELDA, asfaltoA, asfaltoB);
    }
    // bordillo amarillo arriba y blanco abajo
    DrawRectangle(0, MARGEN_Y + CELDA - 3, VENTANA_W, 3, Color{240, 200, 60, 255});
    int yIni = MARGEN_Y + (FILAS - 1) * CELDA;
    DrawRectangle(0, yIni - 3, VENTANA_W, 3, Color{240, 240, 240, 255});

    // rayas blancas animadas entre carriles
    float desfase = std::fmod(t * 60.0f, 48.0f);
    for (int i = 1; i < CARRILES; i++) {
        int yLinea = MARGEN_Y + (i + 1) * CELDA - 2;
        for (int x = -48; x < VENTANA_W; x += 48)
            DrawRectangle((int)(x + desfase), yLinea, 24, 4, Color{225, 225, 225, 200});
    }

    // inicio: banqueta con flores
    DrawRectangleGradientV(0, yIni, VENTANA_W, CELDA, Color{165, 165, 172, 255}, Color{130, 130, 138, 255});
    for (int dx = 0; dx < VENTANA_W; dx += CELDA)
        DrawRectangle(dx, yIni, 2, CELDA, Color{120, 120, 128, 255});
    for (int x = 0; x < VENTANA_W; x += 64) {
        DrawRectangle(x + 30, yIni + 22, 2, 16, Color{70, 140, 70, 255});
        Color flor = ((x / 64) % 2) ? Color{255, 120, 160, 255} : Color{255, 220, 80, 255};
        DrawCircle(x + 31, yIni + 20, 5.0f, flor);
    }
    DrawText("INICIO", 10, yIni + 14, 18, Color{40, 40, 50, 255});

    // oscuridad progresiva por nivel (se aplica al fondo, no a los autos)
    if (oscuro > 0.0f)
        DrawRectangle(0, MARGEN_Y, VENTANA_W, VENTANA_H - MARGEN_Y, Fade(Color{10, 12, 50, 255}, oscuro * 0.25f));

    // autos
    for (int i = 0; i < CARRILES; i++) {
        const Carril &c = g.carriles[i];
        int yTop = MARGEN_Y + (i + 1) * CELDA;
        for (const Coche &a : c.autos)
            dibujarCoche(a.x * CELDA + 3.0f, (float)yTop + 6.0f, a.len * CELDA - 6.0f,
                         (float)CELDA - 12.0f, a.color, c.dir, a.camion, a.x * 2.0f);
    }

    // autos destruidos: vuelan o se aplastan
    for (const Volador &v : g.voladores) {
        if (v.tipo == 0) {
            dibujarCoche(v.x, v.y, v.w, v.h, v.c, v.dir, v.camion, t * 12.0f);
        } else {
            float prog = 1.0f - v.vida / v.vidaMax;
            float hh = v.h * std::max(0.12f, 1.0f - prog * 0.88f);
            dibujarCoche(v.x, v.y + (v.h - hh), v.w, hh, v.c, v.dir, v.camion, 0.0f);
        }
    }

    // pollo / tumba
    bool parpadea = std::fmod(t, 3.0f) < 0.12f;
    float dist = std::hypot(g.centroX(g.px) - g.visX, g.centroY(g.py) - g.visY);
    bool caminando = dist > 2.0f;
    float paso = caminando ? std::sin(t * 20.0f) : 0.0f;
    float altura = std::sin(std::min(dist / CELDA, 1.0f) * PI) * 10.0f;
    float cx = g.visX, cy = g.visY - altura;

    if (est == GAMEOVER) {
        dibujarTumba(cx, g.visY);
    } else if (est == MUERTE) {
        float e = std::max(0.15f, 1.0f - te * 2.5f);
        DrawEllipse((int)cx, (int)(g.visY + 14), 24, 7, Color{200, 30, 30, 200});
        dibujarPollo(cx, g.visY, e);
    } else if (est == NIVEL || est == VICTORIA) {
        float salto = std::fabs(std::sin(te * 10.0f)) * 16.0f;
        dibujarPollo(cx, g.visY - salto, 1.0f, std::sin(te * 25.0f) * 5.0f);
    } else {
        bool visible = g.invul <= 0 || ((int)(g.invul * 10) % 2 == 0);   // parpadeo al reaparecer
        // al golpear, el pollo agita las alas con fuerza
        float aleteo = g.golpeT > 0.0f ? std::sin(t * 40.0f) * 10.0f : std::sin(t * 10.0f) * 2.0f;
        if (visible) dibujarPollo(cx, cy, 1.0f, aleteo, parpadea, paso);
    }

    // particulas
    for (const Particula &p : g.part) {
        float a = std::max(0.0f, p.vida / p.vidaMax);
        if (p.tipo == 1) {
            DrawRectangle((int)p.x, (int)p.y, (int)p.tam, (int)p.tam + 2, Fade(p.c, a));
        } else if (p.tipo == 2) {
            DrawEllipse((int)p.x, (int)p.y, p.tam, p.tam * 0.5f, Fade(p.c, a));
        } else if (p.tipo == 3) {        // fuego: se encoge mientras se apaga
            DrawCircle((int)p.x, (int)p.y, p.tam * (0.4f + 0.6f * a), Fade(p.c, a));
        } else if (p.tipo == 4) {        // humo: crece y se desvanece
            DrawCircle((int)p.x, (int)p.y, p.tam * (1.5f - 0.5f * a), Fade(p.c, a * 0.6f));
        } else {
            DrawCircle((int)p.x, (int)p.y, p.tam, Fade(p.c, a));
        }
    }
    // textos flotantes (+10, POW!)
    for (const Popup &p : g.popups) {
        Color c = Fade(p.c, std::max(0.0f, std::min(1.0f, p.vida)));
        int w = MeasureText(p.txt.c_str(), 22);
        DrawText(p.txt.c_str(), (int)p.x - w / 2 + 1, (int)p.y + 1, 22, Fade(BLACK, 0.6f));
        DrawText(p.txt.c_str(), (int)p.x - w / 2, (int)p.y, 22, c);
    }
}

static void dibujarHUD(const Juego &g, int record, float t) {
    DrawRectangleGradientV(0, 0, VENTANA_W, MARGEN_Y, Color{34, 40, 74, 255}, Color{16, 20, 38, 255});
    dibujarNubes(t, 8.0f, 0.10f);
    DrawText("POLLO CRUZA LA CALLE", 20, 10, 28, Color{255, 220, 70, 255});
    for (int i = 0; i < 3; i++) dibujarCorazon(30.0f + i * 34, 62, i < g.vidas);
    DrawText(TextFormat("Nivel %d/%d - %s", g.nivel, NIVEL_FINAL, NOMBRES_NIVEL[g.nivel - 1]),
             300, 16, 22, Color{120, 220, 240, 255});
    DrawText(TextFormat("Puntos: %d", g.puntos), 300, 46, 22, WHITE);

    // golpes disponibles
    DrawText("Golpes:", 300, 74, 16, Color{255, 200, 120, 255});
    for (int k = 0; k < MAX_CARGAS; k++)
        DrawCircle(380 + k * 20, 81, 7.0f, k < g.cargas ? Color{255, 210, 60, 255} : Color{60, 60, 80, 255});

    DrawText(TextFormat("Record: %d", record), 640, 16, 24, Color{255, 220, 70, 255});

    // barra de progreso hacia la meta
    float prog = (float)(FILAS - 1 - g.py) / (float)(FILAS - 1);
    DrawRectangle(640, 52, 280, 14, Color{60, 60, 80, 255});
    DrawRectangle(640, 52, (int)(280 * prog), 14, Color{80, 220, 120, 255});
    DrawRectangleLines(640, 52, 280, 14, Color{220, 220, 230, 255});
    DrawText("Progreso hacia la meta", 640, 70, 16, LIGHTGRAY);

    int yAbajo = MARGEN_Y + FILAS * CELDA;
    DrawRectangleGradientV(0, yAbajo, VENTANA_W, VENTANA_H - yAbajo, Color{16, 20, 38, 255}, Color{8, 10, 20, 255});
    textoCentrado("W/A/S/D o flechas: mover     ESPACIO: golpear     P: pausa     Q: menu",
                  yAbajo + 24, 18, Color{200, 200, 220, 255});
}

static void dibujarMenu(float t, int sel, int record) {
    DrawRectangleGradientV(0, 0, VENTANA_W, VENTANA_H, Color{30, 36, 80, 255}, Color{18, 14, 40, 255});
    // luna
    DrawCircle(VENTANA_W - 140, 110, 44.0f, Color{250, 245, 220, 255});
    DrawCircle(VENTANA_W - 156, 98, 8.0f, Color{220, 215, 190, 255});
    DrawCircle(VENTANA_W - 124, 126, 6.0f, Color{220, 215, 190, 255});
    dibujarNubes(t, 70.0f, 0.12f);
    for (int i = 0; i < 60; i++) {   // estrellas que parpadean
        int sx = (i * 97) % VENTANA_W;
        int sy = 15 + (i * 53) % 200;
        DrawCircle(sx, sy, 1.0f + (i % 2), Fade(WHITE, 0.35f + 0.45f * std::sin(t * 2.0f + i)));
    }
    tituloOnda("POLLO", 30, 110, t, Color{255, (unsigned char)(200 + 55 * std::sin(t * 3.0f)), 60, 255});
    textoCentrado("C R U Z A   L A   C A L L E", 160, 34, Color{90, 220, 240, 255});

    const char *ops[3] = {"JUGAR", "COMO SE JUEGA", "SALIR"};
    for (int i = 0; i < 3; i++) {
        int y = 235 + i * 52;
        if (i == sel) {
            DrawRectangleRounded(Rectangle{VENTANA_W / 2.0f - 200.0f, (float)y - 6.0f, 400.0f, 46.0f}, 0.4f, 8,
                                 Fade(Color{80, 220, 120, 255}, 0.18f + 0.08f * std::sin(t * 6.0f)));
            textoCentrado(TextFormat(">>  %s  <<", ops[i]), y, 32, Color{120, 255, 150, 255});
        } else {
            textoCentrado(ops[i], y, 30, Color{170, 170, 190, 255});
        }
    }
    textoCentrado("W/S o flechas: mover    ENTER: elegir    Q: salir", 395, 20, Color{130, 130, 160, 255});
    textoCentrado(TextFormat("Record: %d", record), 420, 22, Color{255, 220, 70, 255});

    // acera con un pollo paseando y calle con un auto pasando
    DrawRectangle(0, 440, VENTANA_W, 50, Color{150, 150, 158, 255});
    DrawRectangleGradientV(0, 490, VENTANA_W, VENTANA_H - 490, Color{74, 77, 86, 255}, Color{56, 59, 67, 255});
    float px = 60 + (std::sin(t * 0.8f) * 0.5f + 0.5f) * (VENTANA_W - 120);
    float hop = std::fabs(std::sin(t * 8.0f)) * 8.0f;
    dibujarPollo(px, 458 - hop, 1.0f, std::sin(t * 16.0f) * 3.0f, std::fmod(t, 3.0f) < 0.12f,
                 std::sin(t * 16.0f));
    float cxAuto = std::fmod(t * 180.0f, (float)VENTANA_W + 250.0f) - 150.0f;
    dibujarCoche(cxAuto, 498, 110, 34, Color{220, 60, 60, 255}, 1, false, cxAuto * 0.2f);
}

static void dibujarInstrucciones() {
    DrawRectangleGradientV(0, 0, VENTANA_W, VENTANA_H, Color{30, 36, 80, 255}, Color{18, 14, 40, 255});
    textoCentrado("COMO SE JUEGA", 30, 44, Color{255, 220, 70, 255});
    const char *l[] = {
        "Eres un pollo y debes cruzar la calle hasta la META.",
        "Si un auto te toca, pierdes una vida (tienes 3).",
        "Avanzar cada fila nueva da +10. Llegar a la meta da 100 x nivel.",
        "W/A/S/D o flechas: moverte (mantener = avanzar seguido)",
        "ESPACIO o J: GOLPEAR un auto cercano (tienes 5 golpes).",
        "Los golpes se recargan solos, uno cada 8 segundos.",
        "Cuando un auto esta por chocarte, el tiempo se hace lento: pega!",
        "P: pausa     Q: volver al menu",
        "",
        "Nivel 1 FACIL, nivel 2 NORMAL, nivel 3 MEDIO,",
        "nivel 4 DIFICIL y nivel 5 EXTREMO.",
        "Cruza el nivel 5 para ganar!"};
    int n = (int)(sizeof(l) / sizeof(l[0]));
    for (int i = 0; i < n; i++) textoCentrado(l[i], 110 + i * 28, 21, WHITE);
    textoCentrado("Pulsa cualquier tecla para volver...", 490, 20, Color{150, 150, 180, 255});
}

// ------------------------- Programa principal -------------------------
int main() {
    InitWindow(VENTANA_W, VENTANA_H, "El Pollo Cruza la Calle");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    Juego g;
    g.generarCarriles();
    g.reiniciarPollo();
    Estado est = MENU;
    float te = 0, acc = 0, tMenu = 0, tConf = 0, sacudida = 0, cdMov = 0;
    float lento = 0, cdLento = 0;     // tiempo lento activo y espera para volver a activarlo
    int sel = 0, record = cargarRecord();
    bool salirApp = false;

    auto cambiar = [&](Estado e) {
        est = e; te = 0; acc = 0; tConf = 0; lento = 0;
        while (GetKeyPressed() != 0) {}
    };
    auto actualizarRecord = [&]() {
        if (g.puntos > record) { record = g.puntos; guardarRecord(record); }
    };
    auto morir = [&]() { g.vidas--; g.plumas(); cambiar(MUERTE); };
    auto meta = [&]() {
        g.puntos += 100 * g.nivel;
        actualizarRecord();
        if (g.nivel >= NIVEL_FINAL) cambiar(VICTORIA);
        else cambiar(NIVEL);
    };

    while (!WindowShouldClose() && !salirApp) {
        float dt = std::min(GetFrameTime(), 0.1f);
        float t = (float)GetTime();
        te += dt; tMenu += dt;
        cdMov -= dt;
        if (cdLento > 0.0f) cdLento -= dt;
        sacudida = 0;

        // ----- logica por estado -----
        switch (est) {
        case MENU:
            if ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) && sel > 0) sel--;
            if ((IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) && sel < 2) sel++;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (sel == 0) { g.nuevoJuego(); cambiar(CUENTA); }
                else if (sel == 1) cambiar(INSTR);
                else salirApp = true;
            }
            if (IsKeyPressed(KEY_Q)) salirApp = true;
            break;
        case INSTR:
            if (GetKeyPressed() != 0) cambiar(MENU);
            break;
        case CUENTA:
            if (te >= 2.4f) cambiar(JUEGO);
            break;
        case JUEGO: {
            if (IsKeyPressed(KEY_Q)) { actualizarRecord(); cambiar(MENU); break; }
            if (IsKeyPressed(KEY_P)) { cambiar(PAUSA); break; }

            // golpear un auto cercano
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_J)) {
                if (g.golpear()) lento = 0.0f;
            }
            // si un auto esta a punto de chocarte, el tiempo se hace lento para que alcances a pegarle
            if (lento <= 0.0f && cdLento <= 0.0f && g.cargas > 0 && g.invul <= 0.0f && g.peligro()) {
                lento = 1.6f;
                cdLento = 3.0f;
            }

            int dx = 0, dy = 0;
            bool nueva = false;
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))         { dy = -1; nueva = true; }
            else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  { dy = 1;  nueva = true; }
            else if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  { dx = -1; nueva = true; }
            else if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { dx = 1;  nueva = true; }
            else if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))          dy = -1;
            else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))        dy = 1;
            else if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))        dx = -1;
            else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))       dx = 1;

            // al pulsar: movimiento inmediato. Al mantener: avanza cada 0.1 s
            if ((dx || dy) && (nueva || cdMov <= 0)) {
                g.mover(dx, dy);
                cdMov = nueva ? 0.16f : 0.10f;
            }
            break;
        }
        case PAUSA:
            if (IsKeyPressed(KEY_P)) cambiar(JUEGO);
            if (IsKeyPressed(KEY_Q)) { actualizarRecord(); cambiar(MENU); }
            break;
        case MUERTE:
            if (te < 0.5f) sacudida = std::sin(te * 90.0f) * 6.0f;
            if (te >= 1.3f) {
                if (g.vidas <= 0) { actualizarRecord(); cambiar(GAMEOVER); }
                else { g.reiniciarPollo(); cambiar(JUEGO); }
            }
            break;
        case NIVEL:
            tConf -= dt;
            if (tConf <= 0) { tConf = 0.12f; g.confeti(6, g.visX, g.visY, false); }
            if (te >= 1.4f) { g.nuevoNivel(); cambiar(BANNER); }
            break;
        case BANNER:
            if (te >= 1.0f) cambiar(JUEGO);
            break;
        case GAMEOVER:
            if (te > 1.5f && GetKeyPressed() != 0) cambiar(MENU);
            break;
        case VICTORIA:
            tConf -= dt;
            if (tConf <= 0) {
                tConf = 0.07f;
                g.confeti(8, (float)g.ri(40, VENTANA_W - 40), (float)MARGEN_Y, true);
            }
            if (te > 2.0f && GetKeyPressed() != 0) cambiar(MENU);
            break;
        }

        // ----- logica continua (autos, efectos, colisiones) -----
        float ritmo = (est == JUEGO && lento > 0.0f) ? 0.25f : 1.0f;   // tiempo lento
        if (est == JUEGO && lento > 0.0f) lento -= dt;
        if (est != MENU && est != INSTR && est != PAUSA) {
            g.moverAutos(dt * ritmo);
            g.actualizarEfectos(dt, ritmo);
        }
        if (est == JUEGO) {
            if (g.py == 0) meta();
            else if (g.invul <= 0 && g.colision()) morir();
        }

        // ----- dibujo -----
        BeginDrawing();
        if (est == MENU) {
            dibujarMenu(tMenu, sel, record);
        } else if (est == INSTR) {
            dibujarInstrucciones();
        } else {
            ClearBackground(Color{20, 24, 40, 255});
            Camera2D cam{};
            cam.offset = {sacudida, 0};
            cam.zoom = 1.0f;
            BeginMode2D(cam);
            dibujarEscena(g, est, te, t);
            EndMode2D();
            dibujarHUD(g, record, t);

            // tiempo lento: tinte azul y aviso para golpear
            if (est == JUEGO && lento > 0.0f) {
                DrawRectangle(0, MARGEN_Y, VENTANA_W, FILAS * CELDA, Fade(Color{60, 120, 255, 255}, 0.14f));
                int tam = 30 + (int)(3.0f * std::sin(t * 10.0f));
                textoCentrado("GOLPEA! (ESPACIO)", MARGEN_Y + 20, tam, Color{255, 230, 80, 255});
            }

            // destello rojo al morir
            if (est == MUERTE)
                DrawRectangle(0, 0, VENTANA_W, VENTANA_H, Fade(Color{255, 40, 40, 255}, std::max(0.0f, 0.35f - te * 0.5f)));

            int cyMsg = MARGEN_Y + (FILAS * CELDA) / 2 - 30;
            switch (est) {
            case CUENTA:
                if (te < 2.1f) textoCentrado(TextFormat("%d", 3 - (int)(te / 0.7f)), cyMsg - 50, 140,
                                             Color{255, 220, 70, 255});
                else textoCentrado("YA!", cyMsg - 50, 140, Color{80, 230, 120, 255});
                textoCentrado(TextFormat("NIVEL %d - %s", g.nivel, NOMBRES_NIVEL[g.nivel - 1]), cyMsg + 100, 26,
                              Color{255, 220, 70, 255});
                break;
            case PAUSA:
                DrawRectangle(0, MARGEN_Y, VENTANA_W, FILAS * CELDA, Color{0, 0, 0, 150});
                textoCentrado("PAUSA", cyMsg - 20, 80, Color{120, 220, 240, 255});
                textoCentrado("P: continuar     Q: menu", cyMsg + 70, 24, WHITE);
                break;
            case MUERTE:
                textoCentrado("SPLAT!", cyMsg, 80, ((int)(te * 10)) % 2 ? Color{255, 80, 80, 255}
                                                                       : Color{255, 220, 70, 255});
                break;
            case NIVEL:
                textoCentrado(TextFormat("Cruzaste! +%d puntos", 100 * g.nivel), cyMsg, 44,
                              Color{80, 230, 120, 255});
                break;
            case BANNER: {
                int tamB = 60 + (int)(4.0f * std::sin(te * 8.0f));   // el texto pulsa
                textoCentrado(TextFormat(">>>  NIVEL %d  <<<", g.nivel), cyMsg, tamB, Color{90, 220, 240, 255});
                textoCentrado(NOMBRES_NIVEL[g.nivel - 1], cyMsg + 70, 30, Color{255, 220, 70, 255});
                break;
            }
            case GAMEOVER:
                DrawRectangle(0, MARGEN_Y, VENTANA_W, FILAS * CELDA, Color{0, 0, 0, 120});
                textoCentrado("G A M E   O V E R", cyMsg - 20, 70, Color{255, 80, 80, 255});
                textoCentrado(TextFormat("Puntos: %d", g.puntos), cyMsg + 60, 30, WHITE);
                if (te > 1.5f) textoCentrado("Pulsa una tecla para volver al menu", cyMsg + 100, 22,
                                             Color{200, 200, 220, 255});
                break;
            case VICTORIA:
                textoCentrado("GANASTE!", cyMsg - 30, 80,
                              ((int)(te * 6)) % 2 ? Color{255, 220, 70, 255} : Color{80, 230, 120, 255});
                textoCentrado("EL POLLO CRUZO TODAS LAS CALLES!", cyMsg + 55, 30, WHITE);
                textoCentrado(TextFormat("Puntos: %d", g.puntos), cyMsg + 95, 26, Color{255, 220, 70, 255});
                if (te > 2.0f) textoCentrado("Pulsa una tecla para volver al menu", cyMsg + 135, 20,
                                             Color{200, 200, 220, 255});
                break;
            default: break;
            }
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
#include "raylib.h"
#include "raymath.h" // ← ADICIONE ESTA LINHA
#include "rlgl.h"
#include <unordered_map>
#include <cmath>
#include <cstdint>

// ============================================================
// MUNDO DE VOXELS (igual antes)
// ============================================================
static inline uint64_t MakeKey(int x, int y, int z)
{
    return ((uint64_t)(x + 0x40000) << 42) | ((uint64_t)(y + 0x40000) << 21) | (uint64_t)(z + 0x40000);
}
static inline void DecodeKey(uint64_t key, int &x, int &y, int &z)
{
    x = (int)((key >> 42) & 0x1FFFFF) - 0x40000;
    y = (int)((key >> 21) & 0x1FFFFF) - 0x40000;
    z = (int)(key & 0x1FFFFF) - 0x40000;
}

std::unordered_map<uint64_t, Color> world;

inline bool HasVoxel(int x, int y, int z)
{
    return world.count(MakeKey(x, y, z)) > 0;
}
inline void SetVoxel(int x, int y, int z, Color c)
{
    world[MakeKey(x, y, z)] = c;
}
inline void RemoveVoxel(int x, int y, int z)
{
    world.erase(MakeKey(x, y, z));
}

// ============================================================
// DDA RAYCAST (igual antes)
// ============================================================
struct RayHit
{
    bool hit;
    int x, y, z;
    int nx, ny, nz;
};

RayHit RaycastVoxels(Ray ray, int maxSteps = 256)
{
    RayHit r = {false, 0, 0, 0, 0, 0, 0};
    Vector3 pos = ray.position;
    Vector3 dir = ray.direction;

    int cellX = (int)floorf(pos.x);
    int cellY = (int)floorf(pos.y);
    int cellZ = (int)floorf(pos.z);

    int stepX = (dir.x >= 0) ? 1 : -1;
    int stepY = (dir.y >= 0) ? 1 : -1;
    int stepZ = (dir.z >= 0) ? 1 : -1;

    float tMaxX = (fabsf(dir.x) < 1e-6f) ? 1e30f : ((dir.x > 0) ? (cellX + 1 - pos.x) : (pos.x - cellX)) / fabsf(dir.x);
    float tMaxY = (fabsf(dir.y) < 1e-6f) ? 1e30f : ((dir.y > 0) ? (cellY + 1 - pos.y) : (pos.y - cellY)) / fabsf(dir.y);
    float tMaxZ = (fabsf(dir.z) < 1e-6f) ? 1e30f : ((dir.z > 0) ? (cellZ + 1 - pos.z) : (pos.z - cellZ)) / fabsf(dir.z);

    float tDeltaX = (fabsf(dir.x) < 1e-6f) ? 1e30f : 1.0f / fabsf(dir.x);
    float tDeltaY = (fabsf(dir.y) < 1e-6f) ? 1e30f : 1.0f / fabsf(dir.y);
    float tDeltaZ = (fabsf(dir.z) < 1e-6f) ? 1e30f : 1.0f / fabsf(dir.z);

    int nx = 0, ny = 0, nz = 0;

    for (int i = 0; i < maxSteps; i++)
    {
        if (HasVoxel(cellX, cellY, cellZ))
        {
            r.hit = true;
            r.x = cellX;
            r.y = cellY;
            r.z = cellZ;
            r.nx = nx;
            r.ny = ny;
            r.nz = nz;
            return r;
        }
        if (tMaxX < tMaxY && tMaxX < tMaxZ)
        {
            cellX += stepX;
            tMaxX += tDeltaX;
            nx = -stepX;
            ny = 0;
            nz = 0;
        }
        else if (tMaxY < tMaxZ)
        {
            cellY += stepY;
            tMaxY += tDeltaY;
            nx = 0;
            ny = -stepY;
            nz = 0;
        }
        else
        {
            cellZ += stepZ;
            tMaxZ += tDeltaZ;
            nx = 0;
            ny = 0;
            nz = -stepZ;
        }
        if (cellY > 200 || cellY < -100)
            break;
    }
    return r;
}

// ============================================================
// SHADING + FACES VISÍVEIS (igual antes)
// ============================================================
Color Shade(Color c, float f)
{
    return (Color){
        (unsigned char)fminf(255.0f, c.r * f),
        (unsigned char)fminf(255.0f, c.g * f),
        (unsigned char)fminf(255.0f, c.b * f),
        c.a};
}

void DrawVoxelVisibleFaces(int x, int y, int z, Color color)
{
    float x0 = (float)x, x1 = (float)(x + 1);
    float y0 = (float)y, y1 = (float)(y + 1);
    float z0 = (float)z, z1 = (float)(z + 1);

    if (!HasVoxel(x, y + 1, z))
    {
        Color c = Shade(color, 1.00f);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlVertex3f(x0, y1, z0);
        rlVertex3f(x0, y1, z1);
        rlVertex3f(x1, y1, z1);
        rlVertex3f(x1, y1, z0);
    }
    if (!HasVoxel(x + 1, y, z))
    {
        Color c = Shade(color, 0.75f);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlVertex3f(x1, y0, z0);
        rlVertex3f(x1, y1, z0);
        rlVertex3f(x1, y1, z1);
        rlVertex3f(x1, y0, z1);
    }
    if (!HasVoxel(x, y, z + 1))
    {
        Color c = Shade(color, 0.55f);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlVertex3f(x0, y0, z1);
        rlVertex3f(x1, y0, z1);
        rlVertex3f(x1, y1, z1);
        rlVertex3f(x0, y1, z1);
    }
}

// ============================================================
// BILLBOARD COM ASPECT CORRETO (para sprites não quadrados)
// ============================================================
void DrawBillboardAspect(Camera3D cam, Texture2D tex, Vector3 basePos, float worldHeight)
{
    float aspect = (float)tex.width / (float)tex.height;
    float halfW = worldHeight * aspect * 0.5f;
    float halfH = worldHeight * 0.5f;

    Vector3 forward = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, cam.up));
    Vector3 up = Vector3CrossProduct(right, forward);

    Vector3 center = {basePos.x, basePos.y + halfH, basePos.z};

    Vector3 r = Vector3Scale(right, halfW);
    Vector3 u = Vector3Scale(up, halfH);

    Vector3 tl = {center.x - r.x + u.x, center.y - r.y + u.y, center.z - r.z + u.z};
    Vector3 tr = {center.x + r.x + u.x, center.y + r.y + u.y, center.z + r.z + u.z};
    Vector3 br = {center.x + r.x - u.x, center.y + r.y - u.y, center.z + r.z - u.z};
    Vector3 bl = {center.x - r.x - u.x, center.y - r.y - u.y, center.z - r.z - u.z};

    // ⬇️ AS DUAS LINHAS MÁGICAS ⬇️
    rlDisableBackfaceCulling();
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlTexCoord2f(0, 0);
    rlTexCoord2f(0, 0);
    rlVertex3f(tl.x, tl.y, tl.z);
    rlTexCoord2f(0, 1);
    rlVertex3f(bl.x, bl.y, bl.z); // ← trocado
    rlTexCoord2f(1, 1);
    rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(1, 0);
    rlVertex3f(tr.x, tr.y, tr.z); // ← trocado
    rlEnd();
    rlSetTexture(0);
    rlEnableBackfaceCulling(); // restaura pro resto do frame
}

// ============================================================
// MAIN
// ============================================================
int main()
{
    InitWindow(1280, 720, "Voxel - Player");
    SetTargetFPS(60);

    // --- Câmera isométrica (offset fixo do alvo) ---
    float dist = 20.0f;
    float elev = 30.0f * DEG2RAD;
    float azim = 45.0f * DEG2RAD;
    Vector3 isoOffset = {
        dist * cosf(elev) * cosf(azim),
        dist * sinf(elev),
        dist * cosf(elev) * sinf(azim)};

    // --- Jogador ---
    Vector3 playerPos = {0.5f, 0.0f, 0.5f}; // pés no chão
    const float PLAYER_SPEED = 6.0f;        // unidades por segundo
    const float PLAYER_HEIGHT = 2.0f;       // altura visual do sprite

    // Carrega o sprite (ou fallback se não achar)
    Texture2D playerTex = LoadTexture("Avatar1.png");
    if (playerTex.id == 0)
    {
        Image img = GenImageChecked(16, 24, 4, 4, RED, DARKGREEN);
        playerTex = LoadTextureFromImage(img);
        UnloadImage(img);
        TraceLog(LOG_WARNING, "Avatar1.png nao encontrado - usando placeholder");
    }
    // Pixel art: sem filtragem bilinear (senão fica borrado)
    SetTextureFilter(playerTex, TEXTURE_FILTER_POINT);

    // --- Câmera inicial (já olhando para o jogador) ---
    Camera3D camera = {0};
    camera.target = (Vector3){playerPos.x, 1.0f, playerPos.z};
    camera.position = Vector3Add(camera.target, isoOffset);
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 25.0f;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    // --- Paleta ---
    Color palette[] = {
        {100, 150, 200, 255},
        {150, 100, 80, 255},
        {100, 200, 100, 255},
        {200, 200, 200, 255},
        {200, 100, 100, 255},
    };
    const int paletteSize = 5;
    int colorIdx = 0;

    // --- Estado de hover ---
    int hoverX = 0, hoverY = 0, hoverZ = 0;
    bool hasHover = false, hoverOnGround = false;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // ---------- MOVIMENTO DO JOGADOR ----------
        // Direções "na tela" para WASD (mesma convenção de antes)
        Vector3 fwd = {-1.0f / sqrtf(2.0f), 0, -1.0f / sqrtf(2.0f)};
        Vector3 rgt = {1.0f / sqrtf(2.0f), 0, -1.0f / sqrtf(2.0f)};

        Vector3 move = {0, 0, 0};
        if (IsKeyDown(KEY_W))
        {
            move.x += fwd.x;
            move.z += fwd.z;
        }
        if (IsKeyDown(KEY_S))
        {
            move.x -= fwd.x;
            move.z -= fwd.z;
        }
        if (IsKeyDown(KEY_A))
        {
            move.x -= rgt.x;
            move.z -= rgt.z;
        }
        if (IsKeyDown(KEY_D))
        {
            move.x += rgt.x;
            move.z += rgt.z;
        }

        float moveLen = sqrtf(move.x * move.x + move.z * move.z);
        if (moveLen > 0.0f)
        {
            float step = PLAYER_SPEED * dt;
            playerPos.x += (move.x / moveLen) * step;
            playerPos.z += (move.z / moveLen) * step;
        }

        // Snap no chão: acha o voxel mais alto abaixo do jogador
        int gx = (int)floorf(playerPos.x);
        int gz = (int)floorf(playerPos.z);
        float groundY = 0.0f;
        for (int y = 100; y >= 0; y--)
        {
            if (HasVoxel(gx, y, gz))
            {
                groundY = (float)(y + 1);
                break;
            }
        }
        playerPos.y = groundY;

        // ---------- CÂMERA SEGUE O JOGADOR ----------
        // Alvo desejado = jogador + offset em Y (para olhar o tronco, não os pés)
        Vector3 desiredTarget = {playerPos.x, playerPos.y + 1.0f, playerPos.z};
        // Lerp para movimento suave (evita "tremida" quando o jogador sobe degraus)
        float lerpFactor = fminf(1.0f, 10.0f * dt);
        camera.target = Vector3Lerp(camera.target, desiredTarget, lerpFactor);
        // Mantém o ângulo isométrico: posição = alvo + offset constante
        camera.position = Vector3Add(camera.target, isoOffset);

        // ---------- COR SELECIONADA ----------
        for (int i = 0; i < paletteSize; i++)
            if (IsKeyPressed(KEY_ONE + i))
                colorIdx = i;
        Color currentColor = palette[colorIdx];

        // ---------- RAYCAST PARA COLOCAR/REMOVER VOXELS ----------
        Ray ray = GetMouseRay(GetMousePosition(), camera);
        RayHit vhit = RaycastVoxels(ray);

        hasHover = false;
        hoverOnGround = false;
        if (vhit.hit)
        {
            hoverX = vhit.x + vhit.nx;
            hoverY = vhit.y + vhit.ny;
            hoverZ = vhit.z + vhit.nz;
            hasHover = true;
        }
        else
        {
            float BIG = 10000.0f;
            RayCollision ground = GetRayCollisionQuad(ray,
                                                      (Vector3){-BIG, 0, -BIG}, (Vector3){BIG, 0, -BIG},
                                                      (Vector3){BIG, 0, BIG}, (Vector3){-BIG, 0, BIG});
            if (ground.hit)
            {
                hoverX = (int)floorf(ground.point.x);
                hoverY = 0;
                hoverZ = (int)floorf(ground.point.z);
                hasHover = true;
                hoverOnGround = true;
            }
        }

        if (hasHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            if (!HasVoxel(hoverX, hoverY, hoverZ))
                SetVoxel(hoverX, hoverY, hoverZ, currentColor);

        if (vhit.hit && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            RemoveVoxel(vhit.x, vhit.y, vhit.z);

        // =====================================================
        // DESENHO
        // =====================================================
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
        DrawGrid(40, 1.0f);

        // Voxels
        rlDisableBackfaceCulling();
        rlBegin(RL_QUADS);
        int rendered = 0, culled = 0;
        for (auto &pair : world)
        {
            int x, y, z;
            DecodeKey(pair.first, x, y, z);
            if (HasVoxel(x + 1, y, z) && HasVoxel(x, y + 1, z) && HasVoxel(x, y, z + 1))
            {
                culled++;
                continue;
            }
            DrawVoxelVisibleFaces(x, y, z, pair.second);
            rendered++;
        }
        rlEnd();
        rlEnableBackfaceCulling();

        // Preview do voxel
        if (hasHover)
        {
            Vector3 ghost = {hoverX + 0.5f, hoverY + 0.5f, hoverZ + 0.5f};
            Color wire = hoverOnGround ? YELLOW : GREEN;
            DrawCubeWires(ghost, 1.0f, 1.0f, 1.0f, wire);
        }

        // Jogador
        // Depth test ON (voxels à frente ocultam o sprite)
        // Depth write OFF (partes transparentes do PNG não ocultam nada)
        rlDisableDepthMask();
        DrawBillboardAspect(camera, playerTex, playerPos, PLAYER_HEIGHT);
        rlEnableDepthMask();

        EndMode3D();

        // HUD
        DrawText("WASD: mover | Esquerdo: colocar | Direito: remover | 1-5: cor",
                 10, 10, 20, DARKGRAY);
        DrawText(TextFormat("Voxels: %d | visiveis: %d | ocultos: %d",
                            (int)world.size(), rendered, culled),
                 10, 40, 20, DARKGREEN);
        DrawText("Cor:", 10, 70, 20, DARKGRAY);
        DrawRectangle(60, 70, 30, 20, currentColor);

        DrawText(TextFormat("Player: (%.1f, %.1f, %.1f)",
                            playerPos.x, playerPos.y, playerPos.z),
                 10, 100, 20, DARKGRAY);
        DrawFPS(10, 130);
        EndDrawing();
    }

    UnloadTexture(playerTex);
    CloseWindow();
    return 0;
}
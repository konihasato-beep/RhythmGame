#include "raylib.h"
#include "tinyxml2.h"
#include <vector>
#include "json.hpp"
using json = nlohmann::json;
using namespace tinyxml2;
enum Direction { IDLE, LEFT, DOWN, UP, RIGHT};// Directionっていう新しく勝手に作った型。整数(int)に名前をつけてる。
struct Frame {
    int x, y, w, h;
    int fx, fy, fw, fh; // frameX, frameY, frameWidth, frameHeight
};
struct Note {
    float time;          // ノーツ開始時間（ms）
    Direction dir;       // 方向
    float sustain;       // 長押しの長さ（ms）
    bool hit;            // 判定済み
    bool active;         // 画面に出ているか
    float y;             // 描画位置
    bool holding;        // 長押し中か
    float holdProgress;  // 長押しの進行度
    bool headActive;   // ノーツの頭（本体）が表示されているか
    bool tailActive;   // 長押しバーが表示されているか
};
struct AttackFrame {
    Rectangle src;   // x,y,width,height
    int frameX, frameY;
    int frameWidth, frameHeight;
    float duration;  // 1フレームの時間（ms）
};
struct AttackAnimation {
    std::vector<AttackFrame> frames;
    Texture2D texture;
};
struct Attack {
    float time;        // 出現タイミング
    float x, y;        // 表示位置
    std::string type;  // "a_hashira"
    bool active = false;
    bool spawned = false;
    int frameIndex = 0;
    float timer = 0;
    float scale = 1.0f;
};
std::vector<Note> notes;
std::vector<Attack> attacks;
std::vector<Frame> anim[5];
std::vector<Frame> animIdle;
std::vector<Frame> animFail[5];
std::string judgeText = "";//std::stringは好きな長さの文字列を扱える型。文字列の変数はこうするしかない。
std::map<std::string, AttackAnimation> attackAnimations;

// TEST 用キャラ
    Texture2D tex[5];//Texture2Dはraylibでの画像専用の形。Texは変数名。
    Texture2D texFail[5];
    Texture2D texNotes[4];  // 0:DOWN, 1:LEFT, 2:RIGHT, 3:UP
    Vector2 playerPos;//Vector2は2D座標を表す型（x, y）。playerPosは変数名。
    //Vector2はfloatじゃないと入れれない。座標を中心にするための計算結果をfloatにしてる。
    Vector2 startPos;     // 移動開始位置
    Vector2 targetPos;    // 移動終了位置
    Texture2D idleTex;
    float judgeTimer = 0.0f;
    Color judgeColor = WHITE;
    float moveTime = 0.0f; // 経過時間
    float moveDuration = 0.2f; // 移動にかける時間（イージングの長さ）
    bool isMoving = false; // 移動中かどうか
    bool playingWalk = false;//アニメーション再生中かどうか。
    float speed = 130.0f;//fはfloat型ってこと。何も書かないとdoubleになる。speedはキャラの速度。
    float songTime = 0;
    float spawnOffset = 1800;   // ノーツが出現する早さ（ms）
    float noteSpeed = 400;     // ノーツの落下速度(px/sec)
    float judgeLine = 870;     // 判定ラインのY座標
    float judgeRange = 200;     // 判定幅
    float player_hp = 100.0f;
    Direction dir = IDLE;//Directionは配列(列挙)名
    int frame = 0;
    float frameTime = 0.06f;
    float timer = 0.0f;
    bool isFailAnim = false;
    Music bgm;

void LoadAnimationXML(const char* xmlPath) {
    XMLDocument doc;
    if (doc.LoadFile(xmlPath) != XML_SUCCESS) return;

    XMLElement* atlas = doc.FirstChildElement("TextureAtlas");
    if (!atlas) return;
    XMLElement* sub = atlas->FirstChildElement("SubTexture");

    while (sub) {//sub は <SubTexture> を指すポインタ
        const char* name = sub->Attribute("name");//変数nameにXMLのname属性の文字列を入れる
        Frame f;
        sub->QueryIntAttribute("x", &f.x);//xの値をFrame(f)グループのxの項目に入れる
        sub->QueryIntAttribute("y", &f.y);//&はポインタ
        sub->QueryIntAttribute("width", &f.w);
        sub->QueryIntAttribute("height", &f.h);

        sub->QueryIntAttribute("frameX", &f.fx);
        sub->QueryIntAttribute("frameY", &f.fy);
        sub->QueryIntAttribute("frameWidth", &f.fw);
        sub->QueryIntAttribute("frameHeight", &f.fh);//push_backはvector の一番うしろにデータを追加する関数

        if (strncmp(name, "player_right ", 13) == 0) anim[RIGHT].push_back(f);
        else if (strncmp(name, "player_left ", 12) == 0) anim[LEFT].push_back(f);
        else if (strncmp(name, "player_up ", 10) == 0)   anim[UP].push_back(f);
        else if (strncmp(name, "player_down ", 12) == 0) anim[DOWN].push_back(f);

        sub = sub->NextSiblingElement("SubTexture");
    }
}

void LoadAnimationXML_Idle(const char* xmlPath) {
    XMLDocument doc;
    doc.LoadFile(xmlPath);

    XMLElement* atlas = doc.FirstChildElement("TextureAtlas");
    XMLElement* sub = atlas->FirstChildElement("SubTexture");

    while (sub) {
        Frame f;
        sub->QueryIntAttribute("x", &f.x);
        sub->QueryIntAttribute("y", &f.y);
        sub->QueryIntAttribute("width", &f.w);
        sub->QueryIntAttribute("height", &f.h);

        sub->QueryIntAttribute("frameX", &f.fx);
        sub->QueryIntAttribute("frameY", &f.fy);
        sub->QueryIntAttribute("frameWidth", &f.fw);
        sub->QueryIntAttribute("frameHeight", &f.fh);

        animIdle.push_back(f);

        sub = sub->NextSiblingElement("SubTexture");
    }
}
void LoadAnimationXML_Fail(const char* xmlPath, Direction d) {
    XMLDocument doc;//XMLを扱うための文書オブジェクトを作る
    doc.LoadFile(xmlPath);//XML ファイルを読み込む

    XMLElement* atlas = doc.FirstChildElement("TextureAtlas");//タグ(のポインタ)を探して取得する
    XMLElement* sub = atlas->FirstChildElement("SubTexture");//タグの最初の1つ(のポインタ)を取得する

    while (sub) {//SubTexture をあるだけ全部順番に読み込む
        const char* name = sub->Attribute("name");
        Frame f;//フレーム情報
        sub->QueryIntAttribute("x", &f.x);
        sub->QueryIntAttribute("y", &f.y);
        sub->QueryIntAttribute("width", &f.w);
        sub->QueryIntAttribute("height", &f.h);

        sub->QueryIntAttribute("frameX", &f.fx);
        sub->QueryIntAttribute("frameY", &f.fy);
        sub->QueryIntAttribute("frameWidth", &f.fw);
        sub->QueryIntAttribute("frameHeight", &f.fh);

        if (d == RIGHT  && strncmp(name, "right_fail ",  11) == 0) animFail[d].push_back(f);//name が “migi_fail ” で始まっているか
        if (d == LEFT   && strncmp(name, "left_fail ", 10) == 0) animFail[d].push_back(f);//d は LEFT（＝2）だから animFail[2] に追加される
        if (d == UP     && strncmp(name, "up_fail ", 8) == 0) animFail[d].push_back(f);
        if (d == DOWN   && strncmp(name, "down_fail ",  10) == 0) animFail[d].push_back(f);
        //animFail[d].push_back(f);
        sub = sub->NextSiblingElement("SubTexture");
    }
}
void LoadNotesFromJson(const char* path) {
    notes.clear();

    char* text = LoadFileText(path);
    if (!text) return;

    json j = json::parse(text);
    UnloadFileText(text);

    for (auto& n : j["notes"]) {
        Note note;

        note.time = n["time"];
        note.sustain = n.value("sustain", 0.0f);

        std::string dirStr = n["dir"];

        if (dirStr == "down")  note.dir = DOWN;
        if (dirStr == "left")  note.dir = LEFT;
        if (dirStr == "right") note.dir = RIGHT;
        if (dirStr == "up")    note.dir = UP;

        note.hit = false;
        note.active = false;
        note.y = 0;
        note.holding = false;
        note.holdProgress = 0;

        notes.push_back(note);
    }
}

void PlaySuccessAnim(Direction d) {
    dir = d;
    frame = 0;
    playingWalk = true;
    isFailAnim = false;
}

void PlayFailAnim(Direction d) {
    dir = d;
    frame = 0;
    playingWalk = true;
    isFailAnim = true;
}

void MoveCharacter(Direction d) {
    startPos = playerPos;
    if (d == UP)    targetPos = { playerPos.x, playerPos.y - 100 };
    if (d == DOWN)  targetPos = { playerPos.x, playerPos.y + 100 };
    if (d == LEFT)  targetPos = { playerPos.x - 100, playerPos.y };
    if (d == RIGHT) targetPos = { playerPos.x + 100, playerPos.y };

    moveTime = 0;
    isMoving = true;
}
void CheckHit(Direction d, float songTime) {
    Note* best = nullptr;//ないこともある
    float bestDist = 99999;

    // 判定範囲にいるノーツの中で一番近いものを探す
    for (auto &n : notes) {
        if (n.hit) continue;//判定済みは無視。次のノーツを調べる
        if (n.dir != d) continue;//方向が違うノーツも無視。

        float dist = fabs(n.y - judgeLine);//fabsは絶対値。つまりノーツと判定ラインの距離(差)。
        if (dist < judgeRange && dist < bestDist) {//↑での差が範囲以内　かつ　今までの最小値より小さい
            best = &n;//ノーツを取っておく
            bestDist = dist;//最小値　更新
        }
    }
    if (best) {// 成功ノーツがあったら(ないときもある)
        best->hit = true;
        best->headActive = false;
        judgeText = "GOOD";
        judgeColor = GREEN;
        judgeTimer = 1.5f;
        PlaySuccessAnim(d);
        MoveCharacter(d);

        // 長押しノーツなら holding 開始
        if (best->sustain > 0) {
            best->holding = true;
            best->holdProgress = 0;
        }
        TraceLog(LOG_INFO, "HIT at %.2f ms", songTime);
        return;
    }
    // judgeText = "MISS";    ノーツないのに押したときもミスにしたいときのコ－ド
    // judgeColor = RED;
    // judgeTimer = 0.4f;

    // player_hp -= 10.0f;
    // if (player_hp < 0) player_hp = 0;

    // PlayFailAnim(d);
    // MoveCharacter(d);

    // TraceLog(LOG_INFO, "MISS at %.2f ms", songTime);
}
void LoadAttackAnimationXML(const char* xmlPath, const char* typeName) {
    XMLDocument doc;
    if (doc.LoadFile(xmlPath) != XML_SUCCESS) return;

    XMLElement* atlas = doc.FirstChildElement("TextureAtlas");
    if (!atlas) return;

    const char* imagePath = atlas->Attribute("imagePath");
    AttackAnimation anim;
    anim.texture = LoadTexture(imagePath);

    XMLElement* sub = atlas->FirstChildElement("SubTexture");
    while (sub) {
        AttackFrame f;

        sub->QueryFloatAttribute("x", &f.src.x);
        sub->QueryFloatAttribute("y", &f.src.y);
        sub->QueryFloatAttribute("width", &f.src.width);
        sub->QueryFloatAttribute("height", &f.src.height);

        sub->QueryIntAttribute("frameX", &f.frameX);
        sub->QueryIntAttribute("frameY", &f.frameY);
        sub->QueryIntAttribute("frameWidth", &f.frameWidth);
        sub->QueryIntAttribute("frameHeight", &f.frameHeight);

        f.duration = 50; // 1フレーム50ms（必要ならJSONで指定可能）

        anim.frames.push_back(f);
        sub = sub->NextSiblingElement("SubTexture");
    }

    attackAnimations[typeName] = anim;
}
void LoadAttacksFromJson(const char* path) {
    attacks.clear();

    char* text = LoadFileText(path);
    if (!text) return;

    json j = json::parse(text);
    UnloadFileText(text);

    for (auto& a : j["attacks"]) {
        Attack atk;
        atk.time = a["time"];
        atk.x = a["x"];
        atk.y = a["y"];
        atk.type = a["type"];
        atk.scale = a.value("scale", 1.0f);
        attacks.push_back(atk);
    }
}
enum GameState {//状態(画面とか)を複数持っとく
    TITLE,    //enumは列挙型って型。配列みたいに幾つかの定数をまとめとく。
    MENU,
    OPTIONS,
    TEST,
    TEST2,
    GAMEPLAY
};

int main() {
    int w = GetMonitorWidth(0);
    int h = GetMonitorHeight(0);
    InitWindow(w, h, "My Game");//PCが一つなら、そのサイズをここで指定してもいい。
    ToggleFullscreen();// 画面フルスクリーン化　　PCによってサイズが異なる。
 // フルスクリーンにした後の正しい画面サイズをとっとく
    int screenWidth  = GetScreenWidth();//あると画面中央に表示とか便利
    int screenHeight = GetScreenHeight();
    playerPos = { screenWidth/2.0f, screenHeight/2.0f };
    SetTargetFPS(60);//FPS（1秒間の更新回数）ゲーム動作の滑らかさ
    GameState currentState = TITLE;//最初の状態をTITLEにする(タイトル画面表示)
    InitAudioDevice();
    bgm = LoadMusicStream("assets/music/play.ogg");   //OGG

    int menuIndex = 0;//menuIndex は現在選択しているメニュー番号
    const char* menuItems[] = {//menuItems はメニューに表示する文字列
        "Start Game",//中身
        "Options",
        "Test",
        "Test2",
        "Exit"
    };
    const int menuCount = 5;//menuCount は項目数

// アニメーション
    LoadAnimationXML("data/player_right.xml");
    LoadAnimationXML("data/player_left.xml");
    LoadAnimationXML("data/player_up.xml");
    LoadAnimationXML("data/player_down.xml");
    LoadAnimationXML_Idle("data/idle.xml");
    LoadAnimationXML_Fail("data/right_fail.xml", RIGHT);
    LoadAnimationXML_Fail("data/left_fail.xml", LEFT);
    LoadAnimationXML_Fail("data/up_fail.xml", UP);
    LoadAnimationXML_Fail("data/down_fail.xml", DOWN);
    LoadAttackAnimationXML("data/a_hashira.xml", "a_hashira");
    LoadAttackAnimationXML("data/a_maru.xml", "a_maru");
    LoadAttacksFromJson("data/attacks.json");

    // TraceLog(LOG_INFO, "Fail RIGHT frames: %d", (int)animFail[RIGHT].size());
    // TraceLog(LOG_INFO, "Fail LEFT  frames: %d", (int)animFail[LEFT].size());
    // TraceLog(LOG_INFO, "Fail UP    frames: %d", (int)animFail[UP].size());
    // TraceLog(LOG_INFO, "Fail DOWN  frames: %d", (int)animFail[DOWN].size());

    tex[RIGHT] = LoadTexture("assets/images/player_right.png");
    tex[LEFT]  = LoadTexture("assets/images/player_left.png");
    tex[UP]    = LoadTexture("assets/images/player_up.png");
    tex[DOWN]  = LoadTexture("assets/images/player_down.png");
    idleTex = LoadTexture("assets/images/idle.png");
    texFail[RIGHT] = LoadTexture("assets/images/right_fail.png");
    texFail[LEFT]  = LoadTexture("assets/images/left_fail.png");
    texFail[UP]    = LoadTexture("assets/images/up_fail.png");
    texFail[DOWN]  = LoadTexture("assets/images/down_fail.png");
    texNotes[0] = LoadTexture("assets/images/left_n.png");
    texNotes[1] = LoadTexture("assets/images/down_n.png");
    texNotes[2] = LoadTexture("assets/images/up_n.png");
    texNotes[3] = LoadTexture("assets/images/right_n.png");

    while (!WindowShouldClose()) {//メインループ（ゲームが動いている間ずっと実行される）
        // --- Update ---///////////////////////////////////////////////////////////////////////////////////////////////
        switch (currentState) {//状態ごとに処理を分ける
        case TITLE:
            if (IsKeyPressed(KEY_ENTER)) {//ENTER を押したらメニュー画面へ移動
                currentState = MENU;
            }
            break;

        case MENU://上下キーでメニュー項目を選択 % menuCount でループする（下に行きすぎても上に戻る）
            if (IsKeyPressed(KEY_DOWN)) menuIndex = (menuIndex + 1) % menuCount;
            if (IsKeyPressed(KEY_UP))   menuIndex = (menuIndex - 1 + menuCount) % menuCount;

            if (IsKeyPressed(KEY_ENTER)) {
                if (menuIndex == 0) {//ENTER で決定Start Game → GAMEPLAY
                    currentState = GAMEPLAY;
                }
                else if (menuIndex == 1) {
                    currentState = OPTIONS;
                }
                else if (menuIndex == 2) {
                    currentState = TEST;
                }
                else if (menuIndex == 3) {
                    currentState = TEST2;
                    LoadNotesFromJson("data/test.json");
                    SeekMusicStream(bgm, 0.0f);
                    PlayMusicStream(bgm);
                    songTime = 0;
                    
                }
                else if (menuIndex == 4) {
                    CloseWindow();//Exit → ゲーム終了
                }
            }
            break;
        case OPTIONS:
            break;
        case TEST: //イージング移動の開始処理
            if (!playingWalk && !isMoving) {
                bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                // --- 失敗モーション（Shift + 矢印） ---
                if (shift) {
                    if (IsKeyPressed(KEY_UP)) {
                        PlayFailAnim(UP);
                        MoveCharacter(UP);
                    }
                    if (IsKeyPressed(KEY_DOWN)) {
                        PlayFailAnim(DOWN);
                        MoveCharacter(DOWN);
                    }
                    if (IsKeyPressed(KEY_LEFT)) {
                        PlayFailAnim(LEFT);
                        MoveCharacter(LEFT);
                    }
                    if (IsKeyPressed(KEY_RIGHT)) {
                        PlayFailAnim(RIGHT);
                        MoveCharacter(RIGHT);
                    }
                    break;
                }

                if (!shift && IsKeyPressed(KEY_UP)) {
                    dir = UP;
                    frame = 0;
                    playingWalk = true;

                    startPos = playerPos;
                    targetPos = { playerPos.x, playerPos.y - 100 };
                    moveTime = 0.0f;
                    isMoving = true;
                }
                if (!shift && IsKeyPressed(KEY_DOWN)) {
                    dir = DOWN;
                    frame = 0;
                    playingWalk = true;

                    startPos = playerPos;
                    targetPos = { playerPos.x, playerPos.y + 100 };
                    moveTime = 0.0f;
                    isMoving = true;
                }
                if (!shift && IsKeyPressed(KEY_LEFT)) {
                    dir = LEFT;
                    frame = 0;
                    playingWalk = true;

                    startPos = playerPos;
                    targetPos = { playerPos.x - 100, playerPos.y };
                    moveTime = 0.0f;
                    isMoving = true;
                }
                if (!shift && IsKeyPressed(KEY_RIGHT)) {
                   dir = RIGHT;
                   frame = 0;
                    playingWalk = true;

                    startPos = playerPos;
                    targetPos = { playerPos.x + 100, playerPos.y };
                    moveTime = 0.0f;
                    isMoving = true;
                }
            }
            if (isMoving) {//イージング移動の更新
                moveTime += GetFrameTime();
                float t = moveTime / moveDuration;
                if (t > 1.0f) t = 1.0f;
                
                float ease = cbrtf(t); // t^(1/3)
                playerPos.x = startPos.x + (targetPos.x - startPos.x) * ease;
                playerPos.y = startPos.y + (targetPos.y - startPos.y) * ease;
                if (t >= 1.0f) {
                    isMoving = false;
                    playingWalk = false;
                    dir = IDLE;
                    frame = 0;
                }
            }
            timer += GetFrameTime();//アニメーション更新
            if (playingWalk) {
                if (timer >= frameTime) {
                    timer = 0;
                    frame++;
                    if (isFailAnim) {
                        if (frame >= animFail[dir].size()) {
                            frame = 0;
                            playingWalk = false;
                            isFailAnim = false;
                            dir = IDLE;
                        }
                    }
                    else {
                        if (frame >= anim[dir].size()) {
                            frame = 0;
                            playingWalk = false;
                            dir = IDLE;
                        }
                    }
                }
            }
            else {
                if (timer >= frameTime) {
                    timer = 0;
                    frame++;
                    if (frame >= animIdle.size()) frame = 0;
                }
            }
        break;
        case TEST2: {
            bool upNow    = IsKeyDown(KEY_UP);
            bool downNow  = IsKeyDown(KEY_X);
            bool leftNow  = IsKeyDown(KEY_Z);
            bool rightNow = IsKeyDown(KEY_RIGHT);
            static bool prevUp=false, prevDown=false, prevLeft=false, prevRight=false;

            bool upPressed    = (upNow    && !prevUp);
            bool downPressed  = (downNow  && !prevDown);
            bool leftPressed  = (leftNow  && !prevLeft);
            bool rightPressed = (rightNow && !prevRight);
            
            prevUp    = upNow;
            prevDown  = downNow;
            prevLeft  = leftNow;
            prevRight = rightNow;
            
            std::vector<Note*> hitsFail;
            UpdateMusicStream(bgm);
            float dt = GetFrameTime();
            songTime += dt * 1000; // ms
            if (judgeTimer > 0) {
                judgeTimer -= GetFrameTime();
                if (judgeTimer < 0) judgeTimer = 0;
            }
            for (auto &atk : attacks) {
                if (!atk.spawned && songTime >= atk.time) {
                    atk.active = true;
                    atk.spawned = true;
                    atk.frameIndex = 0;
                    atk.timer = 0;
                }
            }
            
            for (auto &n : notes) {//notes の中に入っている Note を、順番に全部 n に入れて処理するループ。
                if (!n.active && songTime >= n.time - spawnOffset) {// 出現
                    n.active = true;
                    n.headActive = true;
                    n.tailActive = (n.sustain > 0);
                    n.y = -100;//画面より100px上
                }
                if (n.active && !n.hit) {// 流れる
                    n.y += noteSpeed * dt;
                }
            }
            if (leftPressed)  TraceLog(LOG_INFO, "Pressed LEFT  at %.2f ms", songTime);
            if (downPressed)  TraceLog(LOG_INFO, "Pressed DOWN  at %.2f ms", songTime);
            if (upPressed)    TraceLog(LOG_INFO, "Pressed UP    at %.2f ms", songTime);
            if (rightPressed) TraceLog(LOG_INFO, "Pressed RIGHT at %.2f ms", songTime);
            if (upPressed) CheckHit(UP,songTime);
            if (downPressed) CheckHit(DOWN,songTime);
            if (leftPressed) CheckHit(LEFT,songTime);
            if (rightPressed) CheckHit(RIGHT,songTime);
           
            for (auto &n : notes) {
                if (!n.hit && n.y > judgeLine + judgeRange) {//noteの中のhitの項目がfalse未判定かつy座標が判定ラインを超えた
                    hitsFail.push_back(&n);
                    TraceLog(LOG_INFO, " %.2f ms", songTime);
                    if (player_hp < 0) player_hp = 0;
                }
                if (n.holding) {// 長押し中
                    bool stillPressed =//まだ押してるか
                    (n.dir == UP    && upNow) ||
                    (n.dir == DOWN  && downNow) ||
                    (n.dir == LEFT  && leftNow) ||
                    (n.dir == RIGHT && rightNow);

                    if (stillPressed) {
                        n.holdProgress += dt * 1000;
                        if (n.holdProgress >= n.sustain) {//長押し進行度が長押しの長さを超えたら
                           n.holding = false;//長押し中を解除
                           n.tailActive = false;
                        }
                    } 
                    else {//長押しの途中で離した
                        n.holding = false;//長押し中を解除
                        n.tailActive = false;
                        hitsFail.push_back(&n);
                        if (player_hp < 0) player_hp = 0;
                    }
                }    
            }
            for (auto* n : hitsFail) {
                n->hit = true;
                n->active = false;
                judgeText = "MISS";
                judgeColor = RED;
                judgeTimer = 0.4f;
                player_hp -= 10.0f;
                if (player_hp < 0) player_hp = 0;
                PlayFailAnim(n->dir);
                MoveCharacter(n->dir);
            }
            if (!hitsFail.empty()) TraceLog(LOG_INFO, "Fail count: %d", (int)hitsFail.size());
            //移動更新
            if (isMoving) {
                moveTime += GetFrameTime();
                float t = moveTime / moveDuration;
                if (t > 1.0f) t = 1.0f;

                float ease = cbrtf(t);
                playerPos.x = startPos.x + (targetPos.x - startPos.x) * ease;
                playerPos.y = startPos.y + (targetPos.y - startPos.y) * ease;
                if (t >= 1.0f) {
                    isMoving = false;
                    playingWalk = false;
                    dir = IDLE;
                    frame = 0;
                }
            }//アニメ更新
            timer += GetFrameTime();
            if (playingWalk) {
                if (timer >= frameTime) {
                    timer = 0;
                    frame++;
                    if (isFailAnim) {
                        if (frame >= animFail[dir].size()) {
                            frame = 0;
                            playingWalk = false;
                            isFailAnim = false;
                            dir = IDLE;
                        }
                    } else {
                        if (frame >= anim[dir].size()) {
                            frame = 0;
                            playingWalk = false;
                            dir = IDLE;
                        }
                    }
                }
            }
            else {
                if (timer >= frameTime) {
                    timer = 0;
                    frame++;
                    if (frame >= animIdle.size()) frame = 0;
                }
            }
            for (auto &atk : attacks) {
                if (!atk.active) continue;
                atk.timer += dt * 1000;
                auto &anim = attackAnimations[atk.type];
                if (atk.timer >= anim.frames[atk.frameIndex].duration) {
                    atk.timer = 0;
                    atk.frameIndex++;
                    if (atk.frameIndex >= anim.frames.size()) {
                        atk.active = false;
                    }
                }
            }
        }
        break;

        case GAMEPLAY:
            break;
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {//戻るボタン
            switch (currentState) {
            case MENU:
                currentState = TITLE;
                break;

            case OPTIONS:
                currentState = MENU;
                break;
            case TEST:
                currentState = MENU;
                break;

            case TEST2:
                currentState = MENU;
                break;

            case GAMEPLAY:
                currentState = MENU; // ここは後で Pause にしてもOK
                break;

            default:
                break;
            }
        }


        // --- Draw ---////////////////////////////////////////////////////////////////////////////////////////////////
        BeginDrawing();//描画開始
        DrawFPS(10, 10);

        ClearBackground(BLACK);//背景を黒で塗りつぶす
        switch (currentState) {
        case TITLE: {
            const char* title = "MY GAME";
            int titleSize = 60;
            int titleWidth = MeasureText(title, titleSize);
            DrawText(title, (screenWidth - titleWidth)/2, screenHeight/3, titleSize, WHITE);

            const char* msg = "Press ENTER";
            int msgSize = 20;
            int msgWidth = MeasureText(msg, msgSize);
            DrawText(msg, (screenWidth - msgWidth)/2, screenHeight/2, msgSize, GRAY);
        } break;

        case MENU:
            for (int i = 0; i < menuCount; i++) {//メニュー項目をループで描画
                int textWidth = MeasureText(menuItems[i], 30);
                Color color = (i == menuIndex) ? YELLOW : WHITE;

                DrawText(menuItems[i],
                         (screenWidth - textWidth)/2,
                         screenHeight/2 + i * 50,
                         30,
                         color);
            }
            break;
        
        case GAMEPLAY:
            DrawText("GAMEPLAY!", 300, 200, 40, GREEN);
            break;
        
        case TEST:{
            Frame fr;
            if (dir == IDLE) {
                fr = animIdle[frame];
            } 
            else if (isFailAnim) {
                fr = animFail[dir][frame];
            }
            else {
                fr = anim[dir][frame];
            }

            Rectangle src = {
                (float)fr.x,
                (float)fr.y,
                (float)fr.w,
                (float)fr.h
            };

             Rectangle dst = {
                playerPos.x + fr.fx,
                playerPos.y + fr.fy,
                (float)fr.fw,
                (float)fr.fh
            };
            if (dir == IDLE) {
                DrawTexturePro(idleTex, src, dst,
                    { fr.fw/2.0f, fr.fh/2.0f }, 0, WHITE);
            } 
            else if (isFailAnim) {
                DrawTexturePro(texFail[dir], src, dst, { fr.fw/2.0f, fr.fh/2.0f }, 0, WHITE);
            }
            else {
                    DrawTexturePro(tex[dir], src, dst,
                        { fr.fw/2.0f, fr.fh/2.0f }, 0, WHITE);
                    }
            break;}
        case TEST2:
            Frame fr;
            DrawLine(0, judgeLine, screenWidth, judgeLine, RED);

            if (dir == IDLE) {
                //if (animIdle.empty()) break;          // 何もなければ描画スキップ
                fr = animIdle[frame];
            }
            else if (isFailAnim) {
                //if (animFail[dir].empty()) break;     // 失敗モーションが無ければスキップ
                if (frame >= animFail[dir].size()) frame = 0;
                fr = animFail[dir][frame];
            }
            else {
                //if (anim[dir].empty()) break;         // 成功モーションが無ければスキップ
                if (frame >= anim[dir].size()) frame = 0;
                fr = anim[dir][frame];
            }
            Rectangle src = {
                (float)fr.x,
                (float)fr.y,
                (float)fr.w,
                (float)fr.h
            };
            Rectangle dst = {
                playerPos.x + fr.fx,
                playerPos.y + fr.fy,
                (float)fr.w,
                (float)fr.h
            };
            
            if (dir == IDLE) {
                 DrawTexturePro(idleTex, src, dst, { fr.fw/2.0f, fr.fh/2.0f }, 0, WHITE);
            }
            else if (isFailAnim) {
                DrawTexturePro(texFail[dir], src, dst, { fr.fw/2.0f, fr.fh/2.0f }, 0, WHITE);
            }
            else {
                DrawTexturePro(tex[dir], src, dst, { fr.fw/2.0f, fr.fh/2.0f }, 0, WHITE);
            }

            for (auto &n : notes) {
                if (!n.active) continue;
                int idx = (int)n.dir - 1;   // LEFT=1 → 0, DOWN=2 → 1, UP=3 → 2, RIGHT=4 → 3
                float baseX = (screenWidth - 400) / (4.0f * 2.0f);//画面幅の1/8
                float noteX = (baseX + idx * baseX * 2.0f) + 200;//画面幅の1/4ずつ左にする。
                if (n.headActive) {//ノーツの頭がまだ生きてる
                    DrawTexturePro(
                        texNotes[idx],
                        {0, 0, (float)texNotes[idx].width, (float)texNotes[idx].height}, // 全部
                        {noteX, n.y, 100, 100}, // 表示位置とサイズ100×100
                        {50, 50}, 0, WHITE);}

                if (n.tailActive) {//長押しあり
                    float tailHeight = n.sustain * noteSpeed / 1000.0f;//noteSpeed[px/sec]
                    float tailHeight_t = tailHeight - (n.holdProgress * noteSpeed / 1000.0f);
                    if (tailHeight_t < 0) tailHeight_t = 0;
                    Color sustainColor = {126, 0, 235, 60};
                    if( n.dir %2 == 0 )sustainColor = {0, 200, 125, 60};//ノーツ番号が偶数なら
                    if (n.hit && n.y > judgeLine) {//ノーツが判定済みかつ判定ラインにきたら
                        n.y =  judgeLine;  //ノーツの頭を判定ラインにする
                    }
                    DrawRectangle(
                        (int)(noteX - 30),//左上のｘ座標
                        (int)(n.y - tailHeight_t),
                        60,(int)tailHeight_t,sustainColor);
                }
            }
            timer += GetFrameTime();
            if (playingWalk) {
                if (timer >= frameTime) {//過ぎたら
                    timer = 0;
                    frame++;
                    if (isFailAnim) {
                        if (frame >= animFail[dir].size()) {
                            frame = 0;
                            playingWalk = false;
                            isFailAnim = false;
                            dir = IDLE;
                        }
                    }
                    else {
                        if (frame >= anim[dir].size()) {
                            frame = 0;
                            playingWalk = false;
                            dir = IDLE;
                        }
                    }
                }
            }
            else {
                if (timer >= frameTime) {
                    timer = 0;
                    frame++;
                    if (frame >= animIdle.size()) frame = 0;
                }
            }
            char buf[64];
            sprintf(buf, "singTime: %.2f", songTime / 1000.0f);
            DrawText(buf, 50, 50, 30, YELLOW);
            if (judgeTimer > 0) {
                DrawText(judgeText.c_str(),
                screenWidth/2 - 60,
                judgeLine - 120,40,judgeColor);
            }
            for (auto &atk : attacks) {
                TraceLog(LOG_INFO, "ATTACK type=%s active=%d frame=%d scale=%.2f",
                    atk.type.c_str(), atk.active, atk.frameIndex, atk.scale);

                if (!atk.active) continue;
                auto &anim = attackAnimations[atk.type];
                auto &f = anim.frames[atk.frameIndex];
                float scale = atk.scale;
                Rectangle dst = {
                    atk.x + f.frameX * scale,atk.y + f.frameY * scale,
                    f.frameWidth * scale,f.frameHeight * scale
                };
                DrawTexturePro(anim.texture, f.src, dst, {0,0}, 0, WHITE);
            }
            // HPバーの背景（黒）
            DrawRectangle(50, 100, 300, 20, DARKGRAY);
            float hpWidth = 300 * (player_hp / 100.0f);// HPバーの現在値（緑 → 赤）
            Color hpColor = (player_hp > 30) ? GREEN : RED;
            DrawRectangle(50, 100, (int)hpWidth, 20, hpColor);
            char hpbuf[32];// HP数値表示
            sprintf(hpbuf, "HP: %.0f", player_hp);
            DrawText(hpbuf, 50, 130, 20, WHITE);

            break;
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

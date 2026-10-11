#include "efx/changelog.h"

namespace efx::changelog {

namespace {

// Die neueste Fassung zuerst. Je Sprache dieselben Stichpunkte in derselben
// Reihenfolge (testChangelog zaehlt nach). Menuenamen so, wie sie in der
// jeweiligen Sprache im Programm stehen.
const std::vector<Entry> kEntries = {
    {81, "2026-10-11",
     {
         // English
         "New: changelog window (Help \xE2\x86\x92 Changelog...) in all four languages. After an update it opens once by itself.\n"
         "Lights with size 0 are discarded like in the game (the engine never creates them).",
         // Deutsch
         "Neu: \xC3\x84nderungsliste (Hilfe \xE2\x86\x92 \xC3\x84nderungen...) in allen vier Sprachen. Nach einem Update \xC3\xB6" "ffnet sie sich einmal von selbst.\n"
         "Lichter mit Gr\xC3\xB6\xC3\x9F" "e 0 werden wie im Spiel verworfen (die Engine legt sie nie an).",
         // 中文
         "新增：更新日志窗口（帮助 \xE2\x86\x92 更新日志...），支持全部四种语言。更新后会自动打开一次。\n"
         "尺寸为 0 的灯光与游戏中一样被忽略（引擎从不创建它们）。",
         // 日本語
         "新機能：更新履歴ウィンドウ（ヘルプ \xE2\x86\x92 更新履歴...）。4 言語すべてに対応し、アップデート後に一度だけ自動で開きます。\n"
         "サイズ 0 のライトはゲームと同様に無視されます（エンジンはそれを作成しません）。",
     }},
    {80, "2026-10-07",
     {
         "Draw order like the old EffectsEd (View \xE2\x86\x92 Draw Order like old EffectsEd, on by default): later segments are drawn on top. Switched off, efxed draws like the game (additive GL_ONE GL_ONE always last).\n"
         "Segments are numbered from 0 like in the original (Segment column, \"Unnamed Emitter 7\", messages \"Particle #0\").\n"
         "Sounds play at the same time instead of one after another.\n"
         "Matched to OpenJK: min/max collision as a box, bouncing particles no longer tunnel through walls, nomipmaps, duplicate shader names, missing stage images, TGA formats and colour rounding like the engine.\n"
         "Matched to the original: more than 7 flags per line and curve flags over several lines are read and written correctly. Comments in an .efx are reported on opening (saving drops them, as in the original).\n"
         "Editing a field during playback no longer pauses it or restarts sounds. Fixed: saving while typing, \"nan\" input, the segment list sorting itself, the game path dialog, recent files with umlauts.\n"
         "Display scaling survives theme changes and moving to another monitor; changing the world scale no longer clips the sky; the start page shows the right images after a new game path.\n"
         "A notice appears when saving or opening fails; a file dropped onto the window waits until an open dialog is closed.\n"
         "The release also offers the .exe files for direct download.",

         "Zeichenreihenfolge wie das alte EffectsEd (Ansicht \xE2\x86\x92 Zeichenreihenfolge wie altes EffectsEd, standardm\xC3\xA4\xC3\x9Fig an): sp\xC3\xA4tere Segmente liegen oben. Ausgeschaltet zeichnet efxed wie das Spiel (additives GL_ONE GL_ONE immer zuletzt).\n"
         "Segmente z\xC3\xA4hlen ab 0 wie im Original (Spalte Segment, \xE2\x80\x9EUnnamed Emitter 7\xE2\x80\x9C, Meldungen \xE2\x80\x9EParticle #0\xE2\x80\x9C).\n"
         "Kl\xC3\xA4nge laufen gleichzeitig statt nacheinander.\n"
         "Abgleich mit OpenJK: Kollision mit min/max als Kasten, Abpraller tunneln nicht mehr durch W\xC3\xA4nde, nomipmaps, doppelte Shadernamen, fehlende Stufenbilder, TGA-Formate und Farbrundung wie die Engine.\n"
         "Abgleich mit dem Original: mehr als 7 Flags je Zeile und Kurven-Flags \xC3\xBC" "ber mehrere Zeilen werden richtig gelesen und geschrieben. Kommentare in einer .efx werden beim \xC3\x96" "ffnen gemeldet (Speichern verliert sie, wie im Original).\n"
         "Ein Feld \xC3\xA4ndern w\xC3\xA4hrend der Wiedergabe h\xC3\xA4lt sie nicht mehr an und startet keinen Klang neu. Behoben: Speichern w\xC3\xA4hrend der Eingabe, \xE2\x80\x9Enan\xE2\x80\x9C-Eingaben, Segmentliste sortiert sich selbst, Spielpfad-Dialog, zuletzt ge\xC3\xB6" "ffnete Dateien mit Umlaut.\n"
         "Die Bildschirmskalierung bleibt beim Theme-Wechsel und auf einem anderen Monitor erhalten; ein neuer Weltma\xC3\x9F" "stab schneidet den Himmel nicht mehr ab; die Startseite zeigt nach neuem Spielpfad die richtigen Bilder.\n"
         "Scheitert Speichern oder \xC3\x96" "ffnen, sagt es ein Hinweis; eine aufs Fenster gezogene Datei wartet, bis ein offener Dialog zu ist.\n"
         "Das Release bietet die .exe-Dateien auch zum direkten Herunterladen.",

         "绘制顺序与旧版 EffectsEd 相同（视图 \xE2\x86\x92 按旧版 EffectsEd 的绘制顺序，默认开启）：后面的片段绘制在上层。关闭后 efxed 按游戏方式绘制（加法混合 GL_ONE GL_ONE 总是最后绘制）。\n"
         "片段与原版一样从 0 开始编号（“片段”列、“Unnamed Emitter 7”、提示信息“Particle #0”）。\n"
         "声音现在同时播放，而不是依次播放。\n"
         "与 OpenJK 对齐：min/max 碰撞按盒体计算，弹跳粒子不再穿墙，nomipmaps、重复的着色器名称、缺失的阶段图像、TGA 格式和颜色取整均与引擎一致。\n"
         "与原版对齐：每行超过 7 个标志以及跨多行的曲线标志可以正确读取和写入。打开 .efx 时会提示其中的注释（保存时注释会丢失，与原版相同）。\n"
         "播放时修改字段不再暂停播放，也不再重新开始声音。已修复：输入时保存、“nan”输入、片段列表自行排序、游戏路径对话框、带变音符的最近文件。\n"
         "切换主题或移动到其他显示器时保留显示缩放；更改世界比例不再裁切天空；设置新游戏路径后起始页显示正确的图像。\n"
         "保存或打开失败时会显示提示；拖到窗口上的文件会等到已打开的对话框关闭后再打开。\n"
         "发布页面现在也提供可直接下载的 .exe 文件。",

         "旧 EffectsEd と同じ描画順（表示 \xE2\x86\x92 旧 EffectsEd の描画順、既定でオン）：後のセグメントが上に描かれます。オフにするとゲームと同じ描画になります（加算 GL_ONE GL_ONE は常に最後）。\n"
         "セグメントはオリジナルと同じく 0 から番号付けされます（セグメント列、「Unnamed Emitter 7」、メッセージ「Particle #0」）。\n"
         "サウンドが順番ではなく同時に再生されます。\n"
         "OpenJK との整合：min/max の衝突をボックスで判定、跳ねるパーティクルが壁を貫通しない、nomipmaps、重複したシェーダー名、欠けたステージ画像、TGA 形式と色の丸めをエンジンと同じに。\n"
         "オリジナルとの整合：1 行に 7 個を超えるフラグや複数行にわたるカーブのフラグを正しく読み書きします。.efx 内のコメントは開くときに通知されます（保存すると失われるのはオリジナルと同じです）。\n"
         "再生中にフィールドを変更しても再生が止まったりサウンドが再開されたりしなくなりました。修正：入力中の保存、「nan」の入力、セグメント一覧の勝手な並べ替え、ゲームパスのダイアログ、ウムラウトを含む最近使ったファイル。\n"
         "テーマ変更時や別のモニターへの移動時も表示スケールを維持。ワールドスケールの変更で空が切れなくなり、新しいゲームパスの後もスタートページに正しい画像が表示されます。\n"
         "保存や読み込みに失敗すると通知が表示されます。ウィンドウにドロップしたファイルは、開いているダイアログが閉じるまで待機します。\n"
         "リリースでは .exe ファイルも直接ダウンロードできます。",
     }},
    {79, "2026-10-02",
     {
         "Windows 7 version (separate package, built with MinGW-w64). It updates itself too and only downloads Windows 7 packages.\n"
         "The updater explicitly requests TLS 1.2 (needed on Windows 7, update KB3140245) and says clearly when that fails.\n"
         "The source code is now public: https://github.com/DennisHerrm/EffectsEd-Remake",

         "Windows-7-Fassung (eigenes Paket, gebaut mit MinGW-w64). Sie aktualisiert sich ebenfalls selbst und l\xC3\xA4" "dt dabei nur Windows-7-Pakete.\n"
         "Der Updater fordert TLS 1.2 ausdr\xC3\xBC" "cklich an (unter Windows 7 n\xC3\xB6tig, Update KB3140245) und sagt verst\xC3\xA4ndlich, wenn es daran scheitert.\n"
         "Der Quelltext ist jetzt \xC3\xB6" "ffentlich: https://github.com/DennisHerrm/EffectsEd-Remake",

         "Windows 7 版本（独立安装包，使用 MinGW-w64 构建）。它同样可以自动更新，并且只下载 Windows 7 安装包。\n"
         "更新程序明确请求 TLS 1.2（Windows 7 需要安装更新 KB3140245），失败时会给出清楚的说明。\n"
         "源代码现已公开：https://github.com/DennisHerrm/EffectsEd-Remake",

         "Windows 7 版（MinGW-w64 でビルドした別パッケージ）。こちらも自動更新され、Windows 7 用のパッケージだけをダウンロードします。\n"
         "アップデーターは TLS 1.2 を明示的に要求し（Windows 7 では更新プログラム KB3140245 が必要）、失敗した場合は分かりやすく知らせます。\n"
         "ソースコードを公開しました：https://github.com/DennisHerrm/EffectsEd-Remake",
     }},
    {78, "2026-10-02",
     {
         "First public version: layout and controls like Raven's EffectsEd (segment list, property pages, playback modes, custom spawn origin, orientation).\n"
         "The preview computes like the engine (OpenJK): emitters, .md3 models, FxRunner, decals, lights, camera shake, shaders with blending, animation and tcMod.\n"
         "Test room at the original size; camera and zoom as in the original.\n"
         "Effects, shaders and images straight from the game's .pk3 files; single .pk3 files can be opened too (File \xE2\x86\x92 Open .pk3 archive...).\n"
         "Library with running previews of every effect in the game.\n"
         "Direct3D 11 or OpenGL; interface in English, German, Chinese and Japanese.\n"
         "Automatic updates (green notice in the status bar, or Help \xE2\x86\x92 Check for Updates...).",

         "Erste \xC3\xB6" "ffentliche Fassung: Aufbau und Bedienung wie Ravens EffectsEd (Segmentliste, Eigenschaftsseiten, Wiedergabe-Modi, eigener Ursprung, Ausrichtung).\n"
         "Die Vorschau rechnet wie die Engine (OpenJK): Emitter, .md3-Modelle, FxRunner, Decals, Licht, Kamerawackeln, Shader mit Mischung, Animation und tcMod.\n"
         "Testraum in Originalgr\xC3\xB6\xC3\x9F" "e; Kamera und Zoom wie im Original.\n"
         "Effekte, Shader und Bilder direkt aus den .pk3 des Spiels; einzelne .pk3 lassen sich auch direkt \xC3\xB6" "ffnen (Datei \xE2\x86\x92 PK3-Archiv \xC3\xB6" "ffnen...).\n"
         "Bibliothek mit laufenden Vorschauen aller Effekte des Spiels.\n"
         "Direct3D 11 oder OpenGL; Oberfl\xC3\xA4" "che auf Englisch, Deutsch, Chinesisch und Japanisch.\n"
         "Automatische Updates (gr\xC3\xBCner Hinweis in der Statuszeile, oder Hilfe \xE2\x86\x92 Nach Updates suchen...).",

         "首个公开版本：布局和操作与 Raven 的 EffectsEd 相同（片段列表、属性页、播放模式、自定义生成原点、方向）。\n"
         "预览按引擎（OpenJK）的方式计算：发射器、.md3 模型、FxRunner、贴花、灯光、镜头震动，以及带混合、动画和 tcMod 的着色器。\n"
         "测试房间为原版尺寸；镜头和缩放与原版相同。\n"
         "直接从游戏的 .pk3 文件读取特效、着色器和图像；也可以直接打开单个 .pk3（文件 \xE2\x86\x92 打开 .pk3 归档...）。\n"
         "特效库，可实时预览游戏中的所有特效。\n"
         "支持 Direct3D 11 或 OpenGL；界面提供英语、德语、中文和日语。\n"
         "自动更新（状态栏中的绿色提示，或 帮助 \xE2\x86\x92 检查更新...）。",

         "最初の公開版：Raven の EffectsEd と同じ構成と操作（セグメント一覧、プロパティページ、再生モード、カスタム生成位置、向き）。\n"
         "プレビューはエンジン（OpenJK）と同じ方法で計算します：エミッター、.md3 モデル、FxRunner、デカール、ライト、カメラの揺れ、ブレンド・アニメーション・tcMod 付きのシェーダー。\n"
         "テストルームはオリジナルと同じ大きさ。カメラとズームもオリジナルと同じです。\n"
         "ゲームの .pk3 からエフェクト、シェーダー、画像を直接読み込みます。単独の .pk3 も開けます（ファイル \xE2\x86\x92 .pk3 アーカイブを開く...）。\n"
         "ゲーム内のすべてのエフェクトを動くプレビューで見られるライブラリ。\n"
         "Direct3D 11 または OpenGL。表示言語は英語、ドイツ語、中国語、日本語。\n"
         "自動更新（ステータスバーの緑色の通知、または ヘルプ \xE2\x86\x92 更新を確認...）。",
     }},
};

}  // namespace

const std::vector<Entry>& entries() { return kEntries; }

std::vector<std::string> points(const Entry& entry, i18n::Language language) {
    const int index = static_cast<int>(language);
    const char* text = index >= 0 && index < 4 ? entry.text[index] : entry.text[0];
    if (!text || !*text) text = entry.text[0];
    std::vector<std::string> out;
    std::string current;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') {
            if (!current.empty()) out.push_back(current);
            current.clear();
        } else {
            current += *p;
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

bool showAfterStart(bool settingsExisted, int lastShownRevision, int currentRevision) {
    return settingsExisted && lastShownRevision < currentRevision;
}

}  // namespace efx::changelog

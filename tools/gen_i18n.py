# -*- coding: utf-8 -*-
# Erzeugt i18n.h und i18n.cpp aus einer Tabelle. Von Hand waere die Gefahr zu
# gross, dass eine Spalte verrutscht.
import os as _os
_ROOT = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))
S = [
 # (Kennung, English, Deutsch, 中文, 日本語)
 ("AppTitle","EffectsEd","EffectsEd","EffectsEd","EffectsEd"),

 # --- Menü Datei ---
 ("MenuFile","File","Datei","文件","ファイル"),
 ("FileNew","New","Neu","新建","新規"),
 ("FileOpen","Open...","Öffnen...","打开...","開く..."),
 ("FileSave","Save","Speichern","保存","保存"),
 ("FileSaveAs","Save As...","Speichern unter...","另存为...","名前を付けて保存..."),
 ("FileClone","Clone Effect","Effekt klonen","克隆特效","エフェクトを複製"),
 ("FileReloadAssets","Reload Assets","Materialien neu laden","重新加载资源","アセットを再読み込み"),
 ("FileRecent","Recent Files","Zuletzt geöffnet","最近的文件","最近使用したファイル"),
 ("FileExit","Exit","Beenden","退出","終了"),

 # --- Menü Bearbeiten ---
 ("MenuEdit","Edit","Bearbeiten","编辑","編集"),
 ("EditUndo","Undo","Rückgängig","撤销","元に戻す"),
 ("EditRedo","Redo","Wiederholen","重做","やり直し"),
 ("EditCut","Cut","Ausschneiden","剪切","切り取り"),
 ("EditCopy","Copy","Kopieren","复制","コピー"),
 ("EditPaste","Paste","Einfügen","粘贴","貼り付け"),
 ("EditDelete","Delete Segment","Segment löschen","删除片段","セグメントを削除"),
 ("EditWallColor","Wall Color...","Wandfarbe...","墙面颜色...","壁の色..."),
 ("EditBgColor","Background Color...","Hintergrundfarbe...","背景颜色...","背景色..."),
 ("EditGamePath","Set Game Path...","Spielpfad festlegen...","设置游戏路径...","ゲームパスを設定..."),
 ("EditPreferences","Preferences...","Einstellungen...","首选项...","環境設定..."),

 # --- Menü Ansicht ---
 ("MenuView","View","Ansicht","视图","表示"),
 ("ViewTheme","Theme","Farbgebung","主题","テーマ"),
 ("ViewLanguage","Language","Sprache","语言","言語"),
 ("ViewRenderer","Renderer","Grafikschnittstelle","渲染器","レンダラー"),
 ("ViewDrawAxes","Draw Axes","Achsen zeichnen","显示坐标轴","軸を表示"),
 ("ViewDrawRoom","Draw Room","Raum zeichnen","显示房间","部屋を表示"),
 ("ViewWireframe","Wireframe","Drahtgitter","线框","ワイヤーフレーム"),
 ("ViewWindVector","Show Wind Vector","Windvektor anzeigen","显示风向量","風ベクトルを表示"),
 ("ViewResetCamera","Reset View","Ansicht zurücksetzen","重置视图","視点をリセット"),
 ("ViewResetLayout","Reset Layout","Aufteilung zurücksetzen","重置布局","レイアウトをリセット"),
 ("ViewScreenshot","Screenshot to File","Bildschirmfoto speichern","截图到文件","スクリーンショットを保存"),
 ("ViewScreenshotClip","Screenshot to Clipboard","Bildschirmfoto kopieren","截图到剪贴板","スクリーンショットをコピー"),
 ("ViewGraphicsInfo","Graphics Driver Info","Grafiktreiber-Informationen","显卡驱动信息","グラフィックドライバ情報"),

 # --- Menü Effekte ---
 ("MenuEffects","Effects","Effekte","特效","エフェクト"),
 ("EffectsNewSegment","New Segment...","Neues Segment...","新建片段...","新規セグメント..."),
 ("EffectsEnabled","Segment Enabled","Segment aktiv","启用片段","セグメント有効"),
 ("EffectsPlay","Play","Abspielen","播放","再生"),
 ("EffectsPause","Pause","Anhalten","暂停","一時停止"),
 ("EffectsStop","Stop","Stopp","停止","停止"),
 ("EffectsRepeat","Repeat","Wiederholen","循环","繰り返し"),
 ("EffectsOrientUp","Orient Up","Nach oben ausrichten","朝上","上向き"),
 ("EffectsOrientSide","Orient Sideways","Seitwärts ausrichten","朝侧面","横向き"),
 ("EffectsValidate","Validate Effect","Effekt prüfen","检查特效","エフェクトを検証"),

 # --- Menü Hilfe ---
 ("MenuHelp","Help","Hilfe","帮助","ヘルプ"),
 ("HelpManual","User Guide","Anleitung","用户指南","ユーザーガイド"),
 ("HelpAbout","About","Über","关于","バージョン情報"),

 # --- Werkzeugleiste ---
 ("ToolWorldScale","World Scale","Weltmaßstab","世界比例","ワールドスケール"),
 ("ToolTimeScale","Time Scale","Zeitmaßstab","时间比例","タイムスケール"),
 ("ToolRepeatRate","Repeat Rate","Wiederholrate","循环间隔","繰り返し間隔"),
 ("ToolClone","Clone the selected segment","Gewähltes Segment klonen","克隆所选片段","選択セグメントを複製"),
 ("ToolUnitsPerFoot","units/foot","Einheiten/Fuß","单位/英尺","単位/フィート"),

 # --- Segmentliste ---
 ("ListName","Name","Name","名称","名前"),
 ("ListType","Type","Typ","类型","タイプ"),
 ("ListSegment","Segment","Segment","片段","セグメント"),
 ("ListDelay","Delay","Verzögerung","延迟","遅延"),
 ("ListCount","Count","Anzahl","数量","数"),
 ("ListEmpty","This effect has no segments yet.","Dieser Effekt hat noch keine Segmente.","此特效还没有片段。","このエフェクトにはまだセグメントがありません。"),

 # --- Eigenschaftenblatt: Reiter ---
 ("TabGeneration","Generation","Erzeugung","生成","生成"),
 ("TabOriginSize","Origin/Size","Lage/Größe","位置/大小","位置/サイズ"),
 ("TabColor","Color","Farbe","颜色","カラー"),
 ("TabMotion","Motion","Bewegung","运动","モーション"),
 ("TabPhysics","Physics","Physik","物理","物理"),
 ("TabPosition","Position","Position","位置","位置"),
 ("TabLine","Line","Linie","线条","ライン"),
 ("TabTail","Tail","Schweif","拖尾","テイル"),
 ("TabModel","Model","Modell","模型","モデル"),
 ("TabEmitter","Emitter","Emitter","发射器","エミッター"),
 ("TabSound","Sound","Klang","声音","サウンド"),
 ("TabFxRunner","FxRunner","FxRunner","特效调用","FxRunner"),
 ("TabCameraShake","CameraShake","CameraShake","CameraShake","CameraShake"),
 ("TabFlags","Flags","Flags","标志","フラグ"),
 ("TabLengthSize2","Length/Size2","Länge/Größe 2","长度/大小 2","長さ/サイズ 2"),
 ("FieldRanged","Range","Spanne","范围","範囲"),
 ("OriginForward","Forward","Vorwärts","前","前方"),
 ("OriginRight","Right","Rechts","右","右"),
 ("OriginUp","Up","Oben","上","上"),
 ("OriginRelativeAxis","Relative to effect axis","Relativ zur Effektachse","相对于特效轴","エフェクト軸に対する相対位置"),
 ("OriginAxisHint","Unchecked: the values are plain world offsets (cheapOrgCalc)","Nicht angehakt: die Werte sind reine Weltversätze (cheapOrgCalc)","未勾选：数值为世界坐标偏移 (cheapOrgCalc)","オフ: 値はワールド座標のオフセット (cheapOrgCalc)"),
 ("OriginSpecialOffsets","Special offset types","Besondere Verteilung","特殊偏移类型","特殊オフセット"),
 ("OriginSpherical","Spherical / Elliptical","Kugel / Ellipsoid","球形 / 椭圆","球形 / 楕円"),
 ("OriginCylindrical","Cylindrical","Zylinder","圆柱","円柱"),
 ("OriginAxisFromOffset","Set effect axis to offset direction","Effektachse in Versatzrichtung drehen","将特效轴设为偏移方向","エフェクト軸をオフセット方向に"),
 ("GenEvenDelay","Use even delay distribution","Verzögerung gleichmäßig verteilen","均匀分布延迟","遅延を均等に分配"),
 ("ShakeIntensity","Intensity","Stärke","强度","強度"),
 ("ShakeNothing","Nothing","Nichts","无","なし"),
 ("ShakeQuake","Quake","Beben","震动","地震"),
 ("ShakeInsane","Insane","Extrem","极强","極端"),
 ("OriginDepthHack","Always draw on top (use depth hack)","Immer im Vordergrund (depth hack)","始终绘制在最前 (depth hack)","常に最前面に描画 (depth hack)"),
 ("PhysicsImpacts","Impacts","Aufprall","撞击","衝突"),
 ("PhysicsKillOnImpact","Kill effect on impact","Effekt beim Aufprall beenden","撞击时结束特效","衝突時にエフェクトを終了"),
 ("PhysicsPlayOnImpact","Play new effect on impact","Beim Aufprall neuen Effekt starten","撞击时播放新特效","衝突時に新しいエフェクトを再生"),
 ("PhysicsBoundingBox","Bounding Box","Begrenzungsbox","边界框","バウンディングボックス"),
 ("PhysicsExpensive","Use extremely expensive physics","Sehr aufwändige Physik benutzen","使用极耗性能的物理","非常に高負荷な物理を使用"),
 ("AnglePitch","Pitch","Nicken","俯仰","ピッチ"),
 ("AngleYaw","Yaw","Gieren","偏航","ヨー"),
 ("AngleRoll","Roll","Rollen","滚转","ロール"),
 ("ModelAttach","Attach model","Modell anhängen","附加模型","モデルを添付"),
 ("ColorModulateAlpha","Modulate RGB value using alpha value","RGB über den Alphawert abmischen","使用透明度调制 RGB","アルファ値で RGB を変調"),
 ("ColorShaderTime","Set shader time for animating textures","Shaderzeit für bewegte Texturen setzen","为动画纹理设置着色器时间","アニメーションテクスチャのシェーダー時間を設定"),
 ("FxRunnerRandomRotation","Rotate randomly around forward axis","Zufällig um die Vorwärtsachse drehen","绕前向轴随机旋转","前方軸まわりにランダム回転"),
 ("EmitterEnable","Emit effects","Effekte aussenden","发射特效","エフェクトを放出"),
 ("LineEndpointGiven","Use specified endpoint","Angegebenen Endpunkt benutzen","使用指定端点","指定した終点を使用"),
 ("LineEndpointTrace","Use trace to find endpoint","Endpunkt durch Trace suchen","通过追踪查找端点","トレースで終点を検出"),
 ("LineEndpointOffset","Use endpoint as offset from origin","Endpunkt als Versatz zum Ursprung","端点作为原点偏移","終点を原点からのオフセットとして使用"),
 ("LineEndpointEffects","Endpoint Effects","Effekte am Endpunkt","端点特效","終点のエフェクト"),
 ("LinePlayAtEndpoint","Play new effect at endpoint","Am Endpunkt neuen Effekt starten","在端点播放新特效","終点で新しいエフェクトを再生"),
 ("LineChaos","Chaos","Chaos","混乱度","カオス"),
 ("LineTaper","Taper electricity from start pt. to end pt.","Blitz zum Endpunkt hin verjüngen","闪电从起点到终点变细","始点から終点へ細くする"),
 ("LineBranch","Enable electricity branching","Verzweigung einschalten","启用闪电分支","分岐を有効にする"),
 ("LineGrow","Grow from start pt. to end pt. during life","Während der Lebensdauer vom Start zum Ende wachsen","生命周期内从起点长到终点","ライフ中に始点から終点へ伸びる"),
 ("MotionAffectedByWind","Affected by wind","Vom Wind beeinflusst","受风影响","風の影響を受ける"),
 ("MotionWindPercent","Percentage (generally 1 to 100)","Anteil (üblich 1 bis 100)","百分比（通常 1 到 100）","割合 (通常 1〜100)"),
 # --- Werkzeugleiste. Die Hilfetexte sind die des Originals, uebersetzt:
 #     sie sagen praeziser, was ein Knopf tut, als jeder Name.
 ("FileRecentEmpty","(none yet)","(noch keine)","（暂无）","（まだありません）"),
 ("MsgScreenshotSaved","Screenshot saved","Bildschirmfoto gespeichert","截图已保存","スクリーンショットを保存しました"),
 ("MsgScreenshotFailed","The graphics backend cannot read the viewport back","Die Grafikschnittstelle kann die Ansicht nicht zurücklesen","图形后端无法回读视口","グラフィックスバックエンドはビューポートを読み戻せません"),
 ("DialogDriverInfo","Graphics Driver Info","Grafiktreiber-Information","显卡驱动信息","グラフィックドライバ情報"),
 ("ViewMainToolbar","Main Toolbar","Hauptleiste","主工具栏","メインツールバー"),
 ("ViewEffectsToolbar","Effects Toolbar","Effektleiste","特效工具栏","エフェクトツールバー"),
 ("ViewPlaybackToolbar","Playback Toolbar","Wiedergabeleiste","播放工具栏","再生ツールバー"),
 ("ViewWorldToolbar","World Toolbar","Weltleiste","世界工具栏","ワールドツールバー"),
 ("ViewDrawGrid","Draw Grid","Gitter anzeigen","显示网格","グリッドを表示"),
 ("ToolDrawAxes","Draw world orientation axes","Weltachsen anzeigen","显示世界坐标轴","ワールド座標軸を表示"),
 ("ToolDrawRoom","Draw the walls of the testing room","Wände des Testraums anzeigen","显示测试房间的墙壁","テストルームの壁を表示"),
 ("ToolDrawGrid","Draw outline of testing room","Umriss des Testraums anzeigen","显示测试房间的轮廓","テストルームの輪郭を表示"),
 ("ToolDrawTextured","Renders effects with textures active","Effekte mit Texturen darstellen","以纹理渲染特效","テクスチャ付きでエフェクトを描画"),
 ("ToolDrawWireframe","Effects are rendered in wireframe","Effekte als Drahtgitter darstellen","以线框渲染特效","エフェクトをワイヤーフレームで描画"),
 ("ToolDrawOverdraw","Shows areas of high polygon overlap","Zeigt Bereiche mit starker Überdeckung","显示高多边形重叠区域","ポリゴンの重なりが多い領域を表示"),
 ("ToolDrawWind","Draw global wind vector","Globalen Windvektor anzeigen","显示全局风向量","グローバル風ベクトルを表示"),
 ("ToolNewSegment","New Segment","Neues Segment","新建片段","新規セグメント"),
 ("UndoNewSegment","new segment","Neues Segment","新建片段","新規セグメント"),
 ("UndoDeleteSegment","segment deleted","Segment gelöscht","已删除片段","セグメント削除"),
 ("UndoCloneSegment","segment cloned","Segment geklont","已克隆片段","セグメント複製"),
 ("UndoEditField","field changed","Feld geändert","字段已更改","フィールド変更"),
 ("ToolDeleteSegment","Delete Segment","Segment löschen","删除片段","セグメントを削除"),
 ("ToolPlaybackSettings","Displays the Playback Settings dialog box","Wiedergabe-Einstellungen öffnen","打开播放设置对话框","再生設定を開く"),
 ("ToolSetOrigin","Set custom FX spawn origin","Eigenen Effektursprung festlegen","设置自定义特效原点","カスタム生成原点を設定"),
 ("TimelineStepBack","One frame back","Ein Bild zurück","后退一帧","1フレーム戻る"),
 ("TimelineStepForward","One frame forward","Ein Bild vorwärts","前进一帧","1フレーム進む"),
 ("TimelineToStart","To the start","An den Anfang","到开头","先頭へ"),
 ("TimelineSpeed","Playback speed","Wiedergabegeschwindigkeit","播放速度","再生速度"),
 ("TimelineFrameRate","Frames per second for stepping","Bilder je Sekunde beim Einzelschritt","单帧步进的帧率","コマ送りのフレームレート"),
 ("TimelineScrub","Drag to move through the effect","Ziehen, um durch den Effekt zu fahren","拖动以浏览特效","ドラッグしてエフェクト内を移動"),
 ("TimelineEndMode","At the end","Am Ende","结束时","終了時"),
 ("EndModeRepeat","Repeat","Wiederholen","重复","繰り返す"),
 ("EndModeHold","Hold last frame","Letztes Bild halten","保持最后一帧","最後のフレームを保持"),
 ("EndModeStop","Stop","Anhalten","停止","停止"),
 ("ToolPlay","Play Effect","Effekt abspielen","播放特效","エフェクトを再生"),
 ("ToolPause","Pause Time","Zeit anhalten","暂停时间","時間を一時停止"),
 ("ToolStop","Stop","Anhalten","停止","停止"),
 ("ViewRoomStyle","Room","Raum","房间","部屋"),
 ("RoomEnclosed","Enclosed room","Geschlossener Raum","封闭房间","閉じた部屋"),
 ("RoomOpenSky","Ground and sky","Boden und Himmel","地面和天空","地面と空"),
 ("RoomNone","Nothing","Nichts","无","なし"),
 ("DialogSun","Sun and Sky","Sonne und Himmel","太阳与天空","太陽と空"),
 ("SunEnabled","Sunlight","Sonnenlicht","阳光","太陽光"),
 ("SunDirection","Direction to the sun","Richtung zur Sonne","朝向太阳的方向","太陽の方向"),
 ("SunAmbient","Ambient light","Grundhelligkeit","环境光","環境光"),
 ("SunDisc","Show the sun","Sonne anzeigen","显示太阳","太陽を表示"),
 ("SunElevation","Elevation","Höhe","高度","高度"),
 ("SunAzimuth","Compass direction","Himmelsrichtung","方位","方位"),
 ("SunNote","Baked into the vertices — the preview has no lighting of its own","In die Eckpunkte gebacken — die Vorschau hat keine eigene Beleuchtung","烘焙到顶点中 — 预览本身没有光照","頂点に焼き込み — プレビュー自体に照明はありません"),
 ("DialogWind","Wind Vector","Windvektor","风向量","風ベクトル"),
 ("WindDirection","Direction","Richtung","方向","方向"),
 ("WindSpeed","Length","Länge","长度","長さ"),
 ("WindDeadNote","Display only: the engine never evaluates wind for effects — the block is commented out in both branches","Nur zur Anzeige: die Engine wertet Wind bei Effekten nirgends aus — der Block ist in beiden Zweigen auskommentiert","仅用于显示：引擎从不为特效计算风 — 两个分支中该代码块均被注释","表示のみ: エンジンはエフェクトに風を適用しません — 両ブランチでコメントアウト済み"),
 ("DialogChooseShaders","Choose Shaders","Shader auswählen","选择着色器","シェーダーを選択"),
 ("ChooseTextures","Textures...","Texturen...","纹理...","テクスチャ..."),
 ("ChoosePreview","Preview","Vorschau","预览","プレビュー"),
 ("ChooseFilter","Filter","Filter","筛选","フィルター"),
 ("ChooseNoGamePath","No game path set — use Edit > Set Default Game Path","Kein Spielpfad gesetzt — Bearbeiten > Spielpfad festlegen","未设置游戏路径 — 编辑 > 设置游戏路径","ゲームパス未設定 — 編集 > ゲームパスの設定"),
 ("ChooseNoneFound","Nothing found","Nichts gefunden","未找到","見つかりません"),
 ("ChooseCount","%d of %d shown","%d von %d angezeigt","显示 %d / %d","%d / %d 件を表示"),
 ("MotionGravityHint","Standard Quake gravity is -800","Die übliche Quake-Schwerkraft ist -800","标准 Quake 重力为 -800","標準の Quake 重力は -800"),
 ("MotionRelativeAxis","Relative to effect axis","Relativ zur Effektachse","相对于特效轴","エフェクト軸に対する相対"),
 ("ColorEmitterNote","The original editor has no colour page for emitters — but FX_AddEmitter does take rgb and alpha, so the engine supports it","Der alte Editor hat für Emitter keine Farbseite — FX_AddEmitter nimmt rgb und alpha aber entgegen, die Engine kann es also","原版编辑器没有发射器的颜色页 — 但 FX_AddEmitter 接受 rgb 和 alpha，引擎是支持的","元のエディタにはエミッター用のカラーページがありませんが、FX_AddEmitter は rgb と alpha を受け取ります"),
 ("LineSharedBits","These three share their bits with useModel, usePhysics and useBBox — in the file they are written as those names","Diese drei teilen sich ihre Bits mit useModel, usePhysics und useBBox — in der Datei stehen sie unter diesen Namen","这三项与 useModel、usePhysics、useBBox 共用位 — 文件中以这些名称写入","この3つは useModel / usePhysics / useBBox とビットを共有し、ファイルにはその名前で書かれます"),
 ("FieldRangedHint","Two values: the engine picks one at random between them","Zwei Werte: die Engine würfelt dazwischen","两个值：引擎在其间随机取值","2つの値: エンジンがその間で乱数を選びます"),
 ("FieldTransition","Transition","Übergang","过渡","トランジション"),
 ("FieldNotUsed","Not evaluated for this type","Wird bei diesem Typ nicht ausgewertet","此类型不会读取","この種類では読み取られません"),
 ("FieldAddEntry","Add","Hinzufügen","添加","追加"),
 ("FieldRemoveEntry","Remove","Entfernen","移除","削除"),
 ("ListChoose","Choose from the game files","Aus den Spieldateien wählen","从游戏文件中选择","ゲームファイルから選択"),
 ("ListTypeHint","Double-click an empty spot to type a name","Doppelklick auf eine freie Stelle: Namen eintippen","双击空白处：输入名称","空いている所をダブルクリック: 名前を入力"),
 ("FieldEmptyList","Empty — the engine needs at least one entry","Leer — die Engine braucht mindestens einen Eintrag","为空 — 引擎至少需要一个条目","空です — エンジンには少なくとも1つ必要です"),
 ("FlagsGroup","Flags","Flags","标志","フラグ"),
 ("SpawnFlagsGroup","Spawn Flags","Spawn-Flags","生成标志","生成フラグ"),
 ("FlagOtherDialect","Only in the other branch","Nur im anderen Zweig","仅在另一分支","別のブランチのみ"),

 # --- Feldbeschriftungen ---
 ("FieldMin","Min","Min","最小","最小"),
 ("FieldMax","Max","Max","最大","最大"),
 ("FieldName","Name","Name","名称","名前"),
 ("FieldCount","Count","Anzahl","数量","数"),
 ("FieldLife","Life","Lebensdauer","生命","ライフ"),
 ("FieldDelay","Delay","Verzögerung","延迟","遅延"),
 ("FieldCullRange","Cull Range","Sichtweite","剔除距离","カリング距離"),
 ("FieldOrigin","Origin","Ursprung","原点","原点"),
 ("FieldOrigin2","Origin 2","Ursprung 2","原点 2","原点 2"),
 ("FieldRadius","Radius","Radius","半径","半径"),
 ("FieldHeight","Height","Höhe","高度","高さ"),
 ("FieldVelocity","Velocity","Geschwindigkeit","速度","速度"),
 ("FieldAcceleration","Acceleration","Beschleunigung","加速度","加速度"),
 ("FieldGravity","Gravity","Schwerkraft","重力","重力"),
 ("FieldRotation","Rotation","Drehung","旋转","回転"),
 ("FieldRotationDelta","Rotation Delta","Drehänderung","旋转变化","回転変化"),
 ("FieldAngles","Angles","Winkel","角度","角度"),
 ("FieldBounce","Bounce / Intensity","Rückprall / Stärke","弹性 / 强度","反発 / 強度"),
 ("FieldWind","Wind","Wind","风","風"),
 ("FieldDensity","Density","Dichte","密度","密度"),
 ("FieldVariance","Variance","Streuung","变化","ばらつき"),
 ("FieldStart","Start","Anfang","起始","開始"),
 ("FieldEnd","End","Ende","结束","終了"),
 ("FieldParm","Parameter","Parameter","参数","パラメータ"),
 ("FieldRgb","RGB","RGB","RGB","RGB"),
 ("FieldAlpha","Alpha","Alpha","透明度","アルファ"),
 ("FieldSize","Size","Größe","大小","サイズ"),
 ("FieldSize2","Size 2","Größe 2","大小 2","サイズ 2"),
 ("FieldLength","Length","Länge","长度","長さ"),
 ("FieldShaders","Shaders","Shader","着色器","シェーダー"),
 ("FieldModels","Models","Modelle","模型","モデル"),
 ("FieldSounds","Sounds","Klänge","声音","サウンド"),
 ("FieldPlayFx","Play Effects","Effekte abspielen","播放特效","エフェクトを再生"),
 ("FieldImpactFx","Impact Effects","Aufprall-Effekte","撞击特效","衝突エフェクト"),
 ("FieldDeathFx","Death Effects","Ende-Effekte","消亡特效","消滅エフェクト"),
 ("FieldEmitFx","Emitted Effects","Ausgesandte Effekte","发射特效","放出エフェクト"),
 ("FieldRepeatDelay","Repeat Delay","Wiederholpause","循环延迟","繰り返し遅延"),

 # --- Übergangsarten ---
 ("CurveLinear","Linear","Linear","线性","リニア"),
 ("CurveNonLinear","Non-Linear","Nicht linear","非线性","ノンリニア"),
 ("CurveClamp","Clamp","Klemmen","钳制","クランプ"),
 ("CurveWave","Wave","Welle","波形","ウェーブ"),
 ("CurveRandom","Random","Zufall","随机","ランダム"),

 # --- Prüfung ---
 ("DiagTitle","Validation","Prüfung","检查","検証"),
 ("DiagError","Error","Fehler","错误","エラー"),
 ("DiagWarning","Warning","Warnung","警告","警告"),
 ("DiagInfo","Note","Hinweis","提示","注記"),
 ("DiagNone","No problems found.","Keine Beanstandungen.","未发现问题。","問題は見つかりませんでした。"),
 ("DiagTargetSp","Single Player","Singleplayer","单人游戏","シングルプレイ"),
 ("DiagTargetMp","Multiplayer","Multiplayer","多人游戏","マルチプレイ"),
 ("DiagTargetBoth","Both","Beide","两者","両方"),

 # --- Statusleiste ---
 ("StatusReady","Ready","Bereit","就绪","準備完了"),
 ("StatusActive","Active","Aktiv","活动","アクティブ"),
 ("StatusDrawn","Drawn","Gezeichnet","已绘制","描画済み"),
 ("StatusScheduled","Scheduled","Geplant","已计划","予約済み"),
 ("StatusMarks","Marks","Spuren","痕迹","マーク"),

 # --- Meldungen ---
 ("MsgUnsavedTitle","Unsaved Changes","Ungespeicherte Änderungen","未保存的更改","未保存の変更"),
 ("MsgUnsavedBody","Save changes before closing?","Änderungen vor dem Schließen speichern?","关闭前保存更改吗？","閉じる前に変更を保存しますか？"),
 ("MsgSave","Save","Speichern","保存","保存"),
 ("MsgDiscard","Discard","Verwerfen","放弃","破棄"),
 ("MsgCancel","Cancel","Abbrechen","取消","キャンセル"),
 ("MsgOk","OK","OK","确定","OK"),
 ("MsgLoadFailed","Could not read the file.","Die Datei ließ sich nicht lesen.","无法读取文件。","ファイルを読み込めませんでした。"),
 ("MsgSaveFailed","Could not write the file.","Die Datei ließ sich nicht schreiben.","无法写入文件。","ファイルを書き込めませんでした。"),
 ("MsgRendererRestart","Changing the renderer restarts the window. Continue?","Der Wechsel der Grafikschnittstelle startet das Fenster neu. Fortfahren?","切换渲染器将重启窗口。是否继续？","レンダラーを変更するとウィンドウが再起動します。続行しますか？"),
 ("MsgRendererFallback","Direct3D 11 is unavailable, using OpenGL instead.","Direct3D 11 steht nicht zur Verfügung, es wird OpenGL benutzt.","Direct3D 11 不可用，改用 OpenGL。","Direct3D 11 が利用できないため、OpenGL を使用します。"),
 # --- Nachgereicht: was der Uebersetzungspruefer gefunden hat ---
 ("PropNoSelection","No effect segment selected.","Kein Segment ausgewählt.","未选择特效片段。","エフェクトセグメントが選択されていません。"),
 ("PropComingSoon","Coming soon","Folgt","即将推出","近日公開"),
 ("ViewportComingSoon","3D view coming soon","3D-Ansicht folgt","3D 视图即将推出","3D ビューは近日公開"),
 ("ListUnnamed","(unnamed)","(ohne Namen)","(未命名)","(名称なし)"),
 ("WindowLog","Log","Protokoll","日志","ログ"),
 ("DialogRenderer","Change Renderer","Grafikschnittstelle wechseln","切换渲染器","レンダラーの変更"),
 ("DialogAbout","About EffectsEd","Über EffectsEd","关于 EffectsEd","EffectsEd について"),
 ("AboutTagline","Effect editing for Jedi Academy","Effektbearbeitung für Jedi Academy","《绝地学院》特效编辑器","Jedi Academy 用エフェクトエディタ"),
 ("AboutSettings","Settings folder","Einstellungsordner","设置文件夹","設定フォルダー"),
 ("MenuFileRecentEmpty","No recent files","Keine zuletzt geöffneten Dateien","没有最近的文件","最近使用したファイルはありません"),
 ("FontMissingCjk","The font for this language is missing; characters may appear as boxes.","Die Schrift für diese Sprache fehlt; Zeichen erscheinen möglicherweise als Kästen.","缺少该语言的字体，字符可能显示为方块。","この言語のフォントがないため、文字が四角で表示される場合があります。"),
 # --- Spielpfad und fehlende Menuepunkte ---
 ("DialogGamePath","Set Default Game Path","Spielpfad festlegen","设置游戏路径","ゲームパスの設定"),
 ("GamePathIntro","This path is used to find shaders, textures, sounds and effect files.","Über diesen Pfad werden Shader, Texturen, Klänge und Effektdateien gefunden.","此路径用于查找着色器、纹理、声音和特效文件。","このパスからシェーダー、テクスチャ、サウンド、エフェクトファイルを検索します。"),
 ("GamePathCurrent","Current game path","Aktueller Spielpfad","当前游戏路径","現在のゲームパス"),
 ("GamePathDefault","Default game path","Voreingestellter Spielpfad","默认游戏路径","既定のゲームパス"),
 ("GamePathBrowse","Browse...","Durchsuchen...","浏览...","参照..."),
 ("GamePathPresets","Presets","Vorlagen","预设","プリセット"),
 ("GamePathValid","Path found","Pfad gefunden","路径有效","パスが見つかりました"),
 ("GamePathMissing","Path does not exist","Pfad existiert nicht","路径不存在","パスが存在しません"),
 ("GamePathNoBase","Folder found, but it does not look like a base folder","Ordner gefunden, sieht aber nicht wie ein base-Ordner aus","找到文件夹，但看起来不像 base 文件夹","フォルダーはありますが、base フォルダーではないようです"),
 ("GamePathContents","Contents","Inhalt","内容","内容"),
 ("MenuViewToolbars","Toolbars","Werkzeugleisten","工具栏","ツールバー"),
 ("ToolbarMain","Main","Haupt","主要","メイン"),
 ("ToolbarEffects","Effects","Effekte","特效","エフェクト"),
 ("ToolbarPlayback","Playback","Wiedergabe","播放","再生"),
 ("ToolbarWorld","World","Welt","世界","ワールド"),
 ("ViewStatusBar","Status Bar","Statusleiste","状态栏","ステータスバー"),
 ("ViewTexturedRoom","Textured Room","Texturierter Raum","带纹理的房间","テクスチャ付きの部屋"),
 ("TextureNone","No Texture","Ohne Textur","无纹理","テクスチャなし"),
 ("TextureBrick","Brick","Ziegel","砖块","レンガ"),
 ("TextureDirt","Dirt","Erde","泥土","土"),
 ("TextureStucco","Stucco","Putz","灰泥","スタッコ"),
 ("ViewRenderOptions","Effect Rendering","Effektdarstellung","特效渲染","エフェクト描画"),
 ("RenderTextured","Textured","Texturiert","带纹理","テクスチャあり"),
 ("RenderWireframe","Wireframe","Drahtgitter","线框","ワイヤーフレーム"),
 ("RenderOverdraw","Overdraw","Überzeichnung","过度绘制","オーバードロー"),
 ("EffectsOrientDown","Orient Down","Nach unten ausrichten","朝下","下向き"),
 ("EffectsCustomOrigin","Set Custom FX Origin...","Eigenen Ursprung festlegen...","设置自定义特效原点...","カスタム原点を設定..."),
 ("EffectsPlaybackSettings","Playback Settings...","Wiedergabe-Einstellungen...","播放设置...","再生設定..."),
 ("EffectsPlaySounds","Play Sounds","Klänge abspielen","播放声音","サウンドを再生"),
 ("EditResetRepeatRate","Reset Repeat Rate On Restart","Wiederholrate beim Start zurücksetzen","重启时重置循环间隔","起動時に繰り返し間隔をリセット"),
 ("EditCloneEffect","Clone Effect","Effekt klonen","克隆特效","エフェクトを複製"),
 ("NotImplemented","Not implemented yet","Noch nicht umgesetzt","尚未实现","未実装"),
 # --- Themen. Bisher deutsch fest verdrahtet in theme.cpp — der
 #     Uebersetzungspruefer sah nur gui/ und hat sie nicht gefunden. ---
 ("ThemeDark","Dark","Dunkel","深色","ダーク"),
 ("ThemeMidnight","Midnight","Mitternacht","午夜","ミッドナイト"),
 ("ThemeClassic","Raven Classic","Raven Klassik","Raven 经典","Raven クラシック"),
 ("ThemeLight","Light","Hell","浅色","ライト"),
 ("ThemeSolarizedDark","Solarized Dark","Solarized Dunkel","Solarized 深色","Solarized ダーク"),
 ("ThemeHighContrast","High Contrast","Hoher Kontrast","高对比度","ハイコントラスト"),
 # --- Weltmassstaebe ---
 # Die Beschriftungen des Originals, Zeichen fuer Zeichen aus dem Binary:
 #   "10 units/foot (WARS)"   und   "16 units/foot (SOF2)"
 #
 # WARS ist Star Wars Jedi Knight — das Spiel, um das es hier geht. Ich hatte
 # den Eintrag uebersehen und 16 faelschlich mit "(JKA, SOF2)" beschriftet;
 # im Original steht dort nur "(SOF2)".
 ("TabStart","Start","Start","开始","スタート"),
 ("StartHeading","Open an effect","Effekt öffnen","打开特效","エフェクトを開く"),
 ("StartNoPath","No game path set yet. Choose one to see the effects that are there.","Noch kein Spielpfad gesetzt. Wähle einen, um die vorhandenen Effekte zu sehen.","尚未设置游戏路径。请选择一个以查看现有特效。","ゲームパスが未設定です。選択すると既存のエフェクトが表示されます。"),
 ("StartSetPath","Set game path…","Spielpfad setzen…","设置游戏路径…","ゲームパスを設定…"),
 ("StartOpenFile","Open .efx file…","EFX-Datei öffnen…","打开 .efx 文件…",".efx ファイルを開く…"),
 ("StartOpenPk3","Open .pk3 archive…","PK3-Archiv öffnen…","打开 .pk3 归档…",".pk3 アーカイブを開く…"),
 ("StartNewEffect","New empty effect","Neuer leerer Effekt","新建空特效","新しい空のエフェクト"),
 ("FileOpenPk3","Open .pk3 archive…","PK3-Archiv öffnen…","打开 .pk3 归档…",".pk3 アーカイブを開く…"),
 ("ArchiveOpened","%s opened: %d effects, %d textures, %d sounds, %d models","%s geöffnet: %d Effekte, %d Bilder, %d Klänge, %d Modelle","已打开 %s：%d 个效果，%d 张图像，%d 个声音，%d 个模型","%s を開きました: エフェクト %d、画像 %d、サウンド %d、モデル %d"),
 ("ArchiveEmpty","%s contains nothing efxed can use (no effects, shaders, images, sounds or models).","%s enthält nichts, was efxed verwenden kann (keine Effekte, Shader, Bilder, Klänge oder Modelle).","%s 中没有 efxed 可用的内容（没有效果、着色器、图像、声音或模型）。","%s には efxed で使えるものがありません（エフェクト、シェーダー、画像、サウンド、モデルなし）。"),
 ("Pk3Loaded","%d effects from the archive","%d Effekte aus dem Archiv","归档中的 %d 个特效","アーカイブ内の %d 件のエフェクト"),
 ("BrowserTitle","Effect browser","Effektbrowser","特效浏览器","エフェクトブラウザー"),
 ("BrowserOpen","Effect browser…","Effektbrowser…","特效浏览器…","エフェクトブラウザー…"),
 ("BrowserFilter","Filter","Filter","筛选","フィルター"),
 ("BrowserCount","%d of %d effects","%d von %d Effekten","%d / %d 个特效","%d / %d 件のエフェクト"),
 ("BrowserEmpty","No effects found. Set a game path first.","Keine Effekte gefunden. Erst einen Spielpfad setzen.","未找到特效。请先设置游戏路径。","エフェクトが見つかりません。先にゲームパスを設定してください。"),
 ("BrowserHint","Double-click opens the effect in a new tab","Doppelklick öffnet den Effekt in einem neuen Reiter","双击可在新标签页中打开特效","ダブルクリックで新しいタブに開きます"),
 ("BrowserSize","Preview size","Vorschaugröße","预览大小","プレビューサイズ"),
 ("BrowserAnimateAll","Animate all","Alle animieren","全部动画","すべて再生"),
 ("SoundPlay","Play sound","Klang abspielen","播放声音","音を再生"),
 ("StopHintLooped","This effect has a repeatDelay. In the game it only repeats when it is played as a looped effect (e.g. by an fx_runner); the old editor wrote repeatDelay into every file.","Dieser Effekt hat einen repeatDelay. Im Spiel wiederholt er sich nur, wenn er als Schleife abgespielt wird (etwa von einem fx_runner); der alte Editor schrieb repeatDelay in jede Datei.","此效果有 repeatDelay。只有作为循环效果播放时（例如由 fx_runner），它才会在游戏中重复；旧编辑器会把 repeatDelay 写入每个文件。","このエフェクトには repeatDelay があります。ゲーム内ではループ効果として再生された場合（fx_runner など）のみ繰り返されます。旧エディタはすべてのファイルに repeatDelay を書き込みました。"),
 ("ViewFront","Front view (looking along +Y)","Ansicht von vorn (Blick entlang +Y)","前视图（沿 +Y 方向）","正面図（+Y 方向）"),
 ("ViewBack","Back view (looking along -Y)","Ansicht von hinten (Blick entlang -Y)","后视图（沿 -Y 方向）","背面図（-Y 方向）"),
 ("ViewLeft","Left view (looking along +X)","Ansicht von links (Blick entlang +X)","左视图（沿 +X 方向）","左側面図（+X 方向）"),
 ("ViewRight","Right view (looking along -X)","Ansicht von rechts (Blick entlang -X)","右视图（沿 -X 方向）","右側面図（-X 方向）"),
 ("ViewTop","Top view (looking down)","Ansicht von oben (Blick nach unten)","顶视图（向下看）","上面図（見下ろし）"),
 ("ViewBottom","Bottom view (looking up)","Ansicht von unten (Blick nach oben)","底视图（向上看）","下面図（見上げ）"),
 ("ViewReset","Reset the view","Ansicht zurücksetzen","重置视图","視点をリセット"),
 ("SettingsGridWalls","Grid on walls and ceiling too","Gitter auch auf Wänden und Decke","墙壁和天花板上也显示网格","壁と天井にもグリッド"),
 ("ShaderFallback","substitute","Ersatz","替代","代替"),
 ("ShaderNoBlock","image found, but no shader block — blending is guessed (alpha). A fire needs additive; a .shader file may be missing from the game path.","Bild gefunden, aber kein Shaderblock — die Mischung wird geraten (alphagemischt). Ein Feuer braucht additiv; vermutlich fehlt eine .shader-Datei im Spielpfad.","找到图像，但没有着色器块 — 混合方式为猜测（透明）。火焰需要叠加；游戏路径中可能缺少 .shader 文件。","画像はあるがシェーダーブロックがない — 合成は推測（アルファ）。炎には加算が必要で、.shader ファイルが不足している可能性があります。"),
 ("BlendAdditive","additive","additiv","叠加","加算"),
 ("BlendAlpha","alpha blended","alphagemischt","透明混合","アルファ合成"),
 ("BlendOpaque","opaque","undurchsichtig","不透明","不透明"),
 ("MenuPreRoll","Start in steady state (as a looped effect)","Eingeschwungen beginnen (wie eine Schleife im Spiel)","以稳定状态开始（如游戏中的循环效果）","定常状態から開始（ゲーム内のループ効果のように）"),
 ("PreRollTip","On: when repeating an effect with repeatDelay, the preview starts with the generations a looped effect would already have built up. Off: it builds up from the first trigger, as in the old editor.","An: beim Wiederholen eines Effekts mit repeatDelay beginnt die Vorschau mit den Generationen, die eine Schleife im Spiel schon aufgebaut hätte. Aus: sie baut sich ab der ersten Auslösung auf, wie im alten Editor.","开启：重复播放带 repeatDelay 的效果时，预览从循环效果已经累积的各代开始。关闭：像旧编辑器一样从第一次触发开始累积。","オン：repeatDelay のあるエフェクトを繰り返すとき、ループ効果ですでに蓄積された世代から開始します。オフ：旧エディタと同様に最初の発動から蓄積します。"),
 ("FieldCountFixed","This type always creates exactly one","Dieser Typ erzeugt immer genau eines","该类型始终只创建一个","この種類は常に 1 つだけ作成します"),
 ("SegmentInsertAbove","Insert segment above","Segment darüber einfügen","在上方插入片段","上に挿入"),
 ("SegmentInsertBelow","Insert segment below","Segment darunter einfügen","在下方插入片段","下に挿入"),
 ("SegmentMoveHint","Drag a row to reorder","Zeile ziehen, um die Reihenfolge zu ändern","拖动行以重新排序","行をドラッグして並べ替え"),
 ("HelpUsersGuide","Original manual (2002)","Originalhandbuch (2002)","原版手册（2002）","オリジナルマニュアル（2002）"),
 ("RowHeightMenu","Row height","Zeilenhöhe","行高","行の高さ"),
 ("RowHeightAuto","Fit to content","An den Inhalt anpassen","适应内容","内容に合わせる"),
 ("RowHeightFixed","Fixed height","Feste Höhe","固定高度","固定の高さ"),
 ("SoundBrowse","Choose sound file...","Klangdatei wählen...","选择声音文件…","音声ファイルを選択…"),
 ("SoundOutsideGame","The file is outside the game path and will not be found by the engine","Die Datei liegt außerhalb des Spielpfads und wird von der Engine nicht gefunden","该文件不在游戏路径内，引擎将无法找到","ファイルはゲームパスの外にあり、エンジンからは見つかりません"),
 ("SoundPlayNone","No sound in the list","Kein Klang in der Liste","列表中没有声音","リストに音がありません"),
 ("SoundPlayOff","Sounds are switched off (Effects > Play sounds)","Klänge sind ausgeschaltet (Effekte > Klänge abspielen)","声音已关闭（特效 > 播放声音）","音はオフです（エフェクト > 音を再生）"),
 ("SettingsOpenLibrary","Open the effect library at startup","Beim Start die Effektbibliothek zeigen","启动时显示特效库","起動時にエフェクトライブラリを表示"),
 ("BrowserSource","Source","Quelle","来源","出典"),
 ("BrowserSourceAll","All sources","Alle Quellen","所有来源","すべての出典"),
 ("GamePathScanSubfolders","Search subfolders for game folders","Unterordner nach Spielordnern durchsuchen","在子文件夹中搜索游戏文件夹","サブフォルダーからゲームフォルダーを探す"),
 ("GamePathFoundRoots","%d game folders found and added","%d Spielordner gefunden und übernommen","找到并添加了 %d 个游戏文件夹","%d 個のゲームフォルダーを追加しました"),
 ("WindowMessages","Messages","Meldungen","消息","メッセージ"),
 ("MessagesNone","No messages ? nothing to complain about","Keine Meldungen ? nichts zu beanstanden","没有消息 ? 一切正常","メッセージなし ? 問題ありません"),
 ("MessagesLine","Line %d","Zeile %d","第 %d 行","%d 行目"),
 ("MessagesClickHint","Click for details","Zum Nachlesen anklicken","点击查看详情","クリックで詳細"),
 ("BrowserTipSegments","%d segments","%d Segmente","%d 个片段","%d セグメント"),
 ("BrowserTipFrom","From: %s","Aus: %s","来自：%s","出典：%s"),
 ("BrowserTipInArchive","In archive: %s","Im Archiv: %s","在归档中：%s","アーカイブ内：%s"),
 ("BrowserTipUnreadable","The file could not be read","Die Datei konnte nicht gelesen werden","无法读取该文件","ファイルを読み取れませんでした"),
 ("BrowserTipNoSegments","No segments ? the file is empty or was not understood","Keine Segmente ? die Datei ist leer oder wurde nicht verstanden","没有片段 ? 文件为空或无法解析","セグメントなし ? ファイルが空か解析できません"),
 ("BrowserTipProblems","%d messages while reading","%d Meldungen beim Lesen","读取时有 %d 条消息","読み込み時に %d 件のメッセージ"),
 ("DialogAssetCounts","%s: %d pk3, %d shaders, %d efx","%s: %d pk3, %d Shader, %d efx","%s：%d 个 pk3，%d 个着色器，%d 个 efx","%s：pk3 %d 個、シェーダー %d 個、efx %d 個"),
 ("StatusHoveredTile","| %s: %d alive, %d drawn, %d vertices","| %s: %d lebend, %d gezeichnet, %d Eckpunkte","| %s：%d 存活，%d 已绘制，%d 顶点","| %s：生存 %d、描画 %d、頂点 %d"),
 ("DriverInfoNone","No probe results available","Keine Erkundungsergebnisse vorhanden","没有可用的检测结果","検出結果がありません"),
 ("ViewTileNumbers","Label preview tiles (diagnosis)","Kacheln beschriften (Diagnose)","标注预览图块（诊断）","タイルに番号を表示（診断）"),
 ("BrowserAnimateAllHint","Off: each preview shows a still frame and only the one under the pointer runs. On: everything runs, which costs a lot of frames.","Aus: jede Vorschau zeigt ein Standbild, nur die unter dem Zeiger läuft. An: alle laufen, das kostet viele Bilder.","关闭：每个预览显示静止画面，仅指针下的会播放。开启：全部播放，非常耗费帧率。","オフ：各プレビューは静止画で、ポインタの下のものだけ再生します。オン：すべて再生し、フレームレートを大きく消費します。"),
 ("LogTitle","Log","Protokoll","日志","ログ"),
 ("LogCopy","Copy all","Alles kopieren","复制全部","すべてコピー"),
 ("LogFollow","Follow","Mitlaufen","自动滚动","自動スクロール"),
 ("LogWarningsOnly","Warnings and errors only","Nur Warnungen und Fehler","仅警告和错误","警告とエラーのみ"),
 ("LogFile","Log file: %s","Protokolldatei: %s","日志文件：%s","ログファイル：%s"),
 ("LogCounters","Browser: %d of %d tiles redrawn, %d draw calls, build %.2f ms, draw %.2f ms","Browser: %d von %d Kacheln neu, %d Zeichenaufrufe, Aufbau %.2f ms, Zeichnen %.2f ms","浏览器：%d / %d 个图块，%d 次绘制调用，构建 %.2f 毫秒，绘制 %.2f 毫秒","ブラウザー：%d / %d タイル、%d 回の描画呼び出し、構築 %.2f ms、描画 %.2f ms"),
 ("BrowserMissing","%d textures not found","%d Texturen nicht gefunden","未找到 %d 个纹理","%d 件のテクスチャが見つかりません"),
 ("BrowserMissingHint","These effects show a soft blob instead of their image. Check the game path and .pk3 files.","Diese Effekte zeigen einen weichen Fleck statt ihres Bildes. Spielpfad und .pk3-Dateien prüfen.","这些特效显示柔和光斑而非其图像。请检查游戏路径和 .pk3 文件。","これらのエフェクトは画像の代わりに柔らかい光点を表示します。ゲームパスと .pk3 ファイルを確認してください。"),
 ("TabUnnamed","(unnamed)","(unbenannt)","(未命名)","(名前なし)"),
 ("TabClose","Close","Schließen","关闭","閉じる"),
 ("TabNew","New tab","Neuer Reiter","新建标签页","新しいタブ"),
 ("TabUnsaved","Unsaved changes","Ungespeicherte Änderungen","未保存的更改","未保存の変更"),
 ("PathsSearched","Search order — the first hit wins","Suchreihenfolge — der erste Treffer gewinnt","搜索顺序 — 第一个匹配项优先","検索順序 — 最初に見つかったものが優先"),
 ("PathsAdd","Add folder","Ordner hinzufügen","添加文件夹","フォルダーを追加"),
 ("PathsRemove","Remove","Entfernen","移除","削除"),
 ("PathsUp","Move up","Nach oben","上移","上へ"),
 ("PathsDown","Move down","Nach unten","下移","下へ"),
 ("PathsBrowse","Browse…","Durchsuchen…","浏览…","参照…"),
 ("PathsHint","Base game, mods and your own folders. A mod placed above the base game overrides its files — like the game does with its .pk3 archives.","Grundspiel, Mods und eigene Ordner. Ein Mod über dem Grundspiel überschreibt dessen Dateien — genau wie das Spiel es mit seinen .pk3-Archiven macht.","基础游戏、模组和自定义文件夹。位于基础游戏之上的模组会覆盖其文件 — 就像游戏处理 .pk3 一样。","ベースゲーム、Mod、独自フォルダー。ベースゲームより上に置いた Mod はそのファイルを上書きします — ゲームが .pk3 を扱うのと同じです。"),
 ("Scale10","10 units/foot (WARS)","10 Einheiten/Fuß (WARS)","10 单位/英尺 (WARS)","10 単位/フィート (WARS)"),
 ("ScaleJka","16 units/foot (SOF2)","16 Einheiten/Fuß (SOF2)","16 单位/英尺 (SOF2)","16 単位/フィート (SOF2)"),
 ("Scale32","32 units/foot","32 Einheiten/Fuß","32 单位/英尺","32 単位/フィート"),
 ("Scale48","48 units/foot","48 Einheiten/Fuß","48 单位/英尺","48 単位/フィート"),
 ("Scale64","64 units/foot","64 Einheiten/Fuß","64 单位/英尺","64 単位/フィート"),
 ("Scale8","8 units/foot","8 Einheiten/Fuß","8 单位/英尺","8 単位/フィート"),
 # --- Wiedergabe-Einstellungen (Dialog 188) ---
 ("DialogPlayback","Playback Settings","Wiedergabe-Einstellungen","播放设置","再生設定"),
 ("PlaybackRepeatMode","Repeat Mode","Wiederholung","循环模式","繰り返しモード"),
 ("PlaybackOnce","Play once","Einmal abspielen","播放一次","一度だけ再生"),
 ("PlaybackUntilStopped","Repeat until stopped","Bis zum Anhalten wiederholen","循环直到停止","停止するまで繰り返す"),
 ("PlaybackForSeconds","Repeat for","Wiederholen für","循环持续","繰り返し時間"),
 ("PlaybackSeconds","seconds","Sekunden","秒","秒"),
 ("PlaybackRateGroup","Repeat Rate","Wiederholrate","循环频率","繰り返し間隔"),
 ("PlaybackEveryFrame","Respawn effect every frame","Effekt jedes Bild neu auslösen","每帧重新生成特效","毎フレーム再生成"),
 ("PlaybackRate","Repeat rate (seconds per spawn)","Rate (Sekunden je Auslösung)","频率（每次生成的秒数）","間隔（生成ごとの秒数）"),
 ("PlaybackFrequency","Repeat frequency (spawns per second)","Frequenz (Auslösungen je Sekunde)","频率（每秒生成次数）","頻度（秒あたりの生成回数）"),
 ("PlaybackTotal","Total repetitions","Auslösungen insgesamt","总循环次数","合計繰り返し回数"),
 ("PlaybackNotApplicable","n/a","n. v.","不适用","該当なし"),
 ("PlaybackMoveGroup","Spawn Point Movement","Bewegung des Ursprungs","生成点移动","生成点の移動"),
 ("PlaybackAnimate","Animate effect spawn location","Ursprung bewegen","动画化特效生成位置","生成位置をアニメート"),
 ("PlaybackVelocity","Velocity","Geschwindigkeit","速度","速度"),
 ("PlaybackResetAfter","Reset location after","Zurücksetzen nach","位置重置于","位置をリセット"),
 # --- Neues Segment (Dialog 157) ---
 ("DialogNewSegment","New Effect Segment","Neues Effektsegment","新建特效片段","新規エフェクトセグメント"),
 ("NewSegmentType","Effect primitive type","Art der Primitive","特效基元类型","エフェクトプリミティブの種類"),
 # --- Ursprung (Dialog 192) ---
 ("DialogSpawnOrigin","Custom Fx Spawn Origin","Eigener Effektursprung","自定义特效生成原点","カスタム生成原点"),
 ("OriginPosition","Position","Lage","位置","位置"),
 ("OriginDefault","Default","Voreinstellung","默认","既定"),
 ("OriginRoomCentre","Room Center","Raummitte","房间中心","部屋の中心"),
 ("OriginOnFloor","On Floor","Auf dem Boden","地板上","床の上"),
 ("OriginOnCeiling","On Ceiling","An der Decke","天花板上","天井"),
 ("OriginOnWall","On Wall","An der Wand","墙上","壁"),
 ("OriginCustom","Custom","Eigene Werte","自定义","カスタム"),
 # --- Farben ---
 ("ColorWall","Wall Color","Wandfarbe","墙面颜色","壁の色"),
 ("ColorBackground","Background Color","Hintergrundfarbe","背景颜色","背景色"),
 ("ColorReset","Reset to theme","Auf Thema zurücksetzen","重置为主题","テーマに戻す"),
 # --- Beschreibungen der Primitivtypen. Standen fest verdrahtet und ohne
 #     Umlaute in src/effect.cpp; der Pruefer sah sie nicht, weil er nur
 #     Zuweisungen an name/label kannte, keine Tabellen. ---
 ("DescParticle","Moving sprite that always faces the camera.","Bewegtes Sprite, immer zur Kamera gedreht.","始终面向摄像机的移动精灵。","常にカメラを向く移動スプライト。"),
 ("DescLine","Straight line to a given or traced endpoint; can trigger an effect there.","Stehende Linie mit angegebenem oder getracetem Endpunkt, kann dort einen Effekt auslösen.","到指定或追踪端点的直线，可在该处触发特效。","指定またはトレースした終点までの直線。そこでエフェクトを発生させられます。"),
 ("DescTail","Moving object with a trailing line.","Bewegtes Objekt mit nachgezogener Linie.","带拖尾线的移动物体。","尾を引く移動オブジェクト。"),
 ("DescCylinder","Upright cylinder with variable size and length.","Stehender Zylinder mit veränderlicher Größe und Länge.","可变大小和长度的立式圆柱。","可変のサイズと長さを持つ直立円柱。"),
 ("DescEmitter","Moving object with optional model, can emit further effects.","Bewegtes Objekt mit optionalem Modell, kann weitere Effekte aussenden.","可带模型的移动物体，可发射更多特效。","モデルを持てる移動オブジェクト。他のエフェクトを放出できます。"),
 ("DescSound","Sound.","Klang.","声音。","サウンド。"),
 ("DescDecal","Projected impact mark.","Projizierte Einschlagspur.","投射的弹痕。","投影される着弾痕。"),
 ("DescOrientedParticle","Moving sprite, freely orientable.","Bewegtes Sprite, frei ausrichtbar.","可自由定向的移动精灵。","自由に向きを設定できる移動スプライト。"),
 ("DescElectricity","Lightning to a given or traced endpoint; can trigger an effect there.","Stehender Blitz mit angegebenem oder getracetem Endpunkt, kann dort einen Effekt auslösen.","到指定或追踪端点的闪电，可在该处触发特效。","指定またはトレースした終点までの稲妻。そこでエフェクトを発生させられます。"),
 ("DescFxRunner","Starts other effects.","Startet andere Effekte.","启动其他特效。","他のエフェクトを開始します。"),
 ("DescLight","Dynamic light source.","Dynamische Lichtquelle.","动态光源。","動的な光源。"),
 ("DescCameraShake","Shakes the camera, strength depends on distance.","Erschüttert die Kamera, Stärke abhängig vom Abstand.","震动摄像机，强度取决于距离。","カメラを揺らします。強さは距離によります。"),
 ("DescScreenFlash","Full-screen flash.","Bildschirmfüllendes Aufblitzen.","全屏闪光。","画面全体のフラッシュ。"),
 # --- Beschriftungen, die noch englisch fest verdrahtet waren ---
 ("CurveNone","(none)","(keiner)","（无）","（なし）"),
 ("FieldAngleDelta","Angle change","Winkeländerung","角度变化","角度の変化"),
 ("GroupElectricity","Electricity","Blitz","闪电","稲妻"),
 ("GroupEnable","Enable","Aktivieren","启用","有効化"),
 # --- Meldungstexte der Pruefung ---------------------------------------
 #
 # Sie waren als einzige einsprachig deutsch. Der Pruefer sah sie nicht,
 # weil er nur Zuweisungen an name/label kannte; erst die zweite Regel
 # (Zeichenketten, die wie ein Satz aussehen) hat sie gefunden.
 #
 # %s und %d stehen fuer eingesetzte Werte. Die Reihenfolge muss in allen
 # vier Sprachen dieselbe sein — ein vertauschtes Paar ergibt Unsinn oder
 # einen Absturz.
 ("VNoPrimitives","The effect contains no primitives.","Der Effekt enthält keine Primitive.","此特效不包含任何基元。","このエフェクトにはプリミティブがありません。"),
 # Zwei Meldungen ausserhalb der Pruefliste, die genauso sichtbar sind.
 ("PathNoAssets","Neither .pk3 files nor shaders/ nor effects/ found.","Weder .pk3-Dateien noch shaders/ noch effects/ gefunden.","未找到 .pk3 文件，也没有 shaders/ 或 effects/。",".pk3 も shaders/ も effects/ も見つかりません。"),
 ("RendererNone","No graphics backend could be created.","Keine Grafikschnittstelle konnte angelegt werden.","无法创建任何图形后端。","グラフィックスバックエンドを作成できませんでした。"),
 # Shaderbezogene Meldungen. Sie werden anders gebaut als die uebrigen
 # (out.push_back statt add), weshalb mein erster Durchgang sie uebersah.
 ("VShaderMissing","\"%s\" is in no .shader file. The engine then looks for an image file of the same name.","\"%s\" steht in keiner .shader-Datei. Die Engine sucht dann eine gleichnamige Bilddatei.","\"%s\" 不在任何 .shader 文件中。引擎将查找同名的图像文件。","\"%s\" はどの .shader にもありません。エンジンは同名の画像ファイルを探します。"),
 ("VNoRgbGen","\"%s\" does not use rgbGen vertex — the primitive's rgb block has no effect.","\"%s\" benutzt kein rgbGen vertex — der rgb-Block der Primitive bleibt wirkungslos.","\"%s\" 未使用 rgbGen vertex — 基元的 rgb 块将无效。","\"%s\" は rgbGen vertex を使っていません — プリミティブの rgb ブロックは効きません。"),
 ("VNoPolygonOffset","\"%s\" has no polygonOffset. Decals without it flicker against the wall.","\"%s\" hat kein polygonOffset. Decals ohne polygonOffset flackern gegen die Wand.","\"%s\" 没有 polygonOffset。缺少它的贴花会与墙面闪烁。","\"%s\" に polygonOffset がありません。無いとデカールが壁とちらつきます。"),
 ("VTooMany","More than 24 primitives (%d). The engine stops at FX_MAX_EFFECT_COMPONENTS and discards the rest.","Mehr als 24 Primitive (%d). Die Engine bricht bei FX_MAX_EFFECT_COMPONENTS ab und verwirft den Rest.","超过 24 个基元（%d）。引擎在 FX_MAX_EFFECT_COMPONENTS 处中止并丢弃其余部分。","プリミティブが24を超えています（%d）。エンジンは FX_MAX_EFFECT_COMPONENTS で打ち切り、残りを破棄します。"),
 ("VNameTooLong","name is %d characters long, the engine cuts at 31.","Name ist %d Zeichen lang, die Engine schneidet bei 31 ab.","名称长度为 %d 个字符，引擎在 31 处截断。","名前が %d 文字あります。エンジンは31文字で切り捨てます。"),
 ("VNameDuplicate","name occurs more than once. GetPrimitiveCopy only ever finds the first.","Name kommt mehrfach vor. GetPrimitiveCopy findet immer nur den ersten.","名称重复。GetPrimitiveCopy 只会找到第一个。","名前が重複しています。GetPrimitiveCopy は最初の1つしか見つけません。"),
 ("VImpactFxNoFlag","impactfx is set but the flag impactRunsFx is missing — the effect will never be started.","impactfx ist gesetzt, aber das Flag impactRunsFx fehlt — der Effekt wird nie gestartet.","已设置 impactfx，但缺少 impactRunsFx 标志 — 该特效永远不会启动。","impactfx が設定されていますが impactRunsFx フラグがありません — このエフェクトは決して開始されません。"),
 ("VImpactFxNoPhysics","impactfx needs usePhysics — without collision there is no impact.","impactfx braucht usePhysics — ohne Kollision gibt es keinen Aufprall.","impactfx 需要 usePhysics — 没有碰撞就没有撞击。","impactfx には usePhysics が必要です — 衝突がなければ着弾もありません。"),
 ("VDeathFxNoFlag","deathfx is set but the flag deathRunsFx is missing — the effect will never be started.","deathfx ist gesetzt, aber das Flag deathRunsFx fehlt — der Effekt wird nie gestartet.","已设置 deathfx，但缺少 deathRunsFx 标志 — 该特效永远不会启动。","deathfx が設定されていますが deathRunsFx フラグがありません — このエフェクトは決して開始されません。"),
 ("VDeathFxKillOnImpact","deathfx with killOnImpact runs when the lifetime ends, but not when the primitive dies on impact.","deathfx mit killOnImpact läuft beim Ablauf der Lebensdauer, aber nicht, wenn die Primitive beim Aufschlag stirbt.","带 killOnImpact 的 deathfx 在寿命结束时运行，但基元因撞击而消亡时不会运行。","killOnImpact 付きの deathfx は寿命の終了時に実行されますが、着弾で消えた場合は実行されません。"),
 ("VNoCurve","%s has a different end value but no transition type — the value stays on the start value for its whole life.","%s hat ein abweichendes Ende, aber keine Übergangsart — der Wert bleibt die ganze Lebensdauer auf dem Startwert.","%s 的结束值不同，但没有过渡类型 — 该值在整个生命周期内保持起始值。","%s の終了値は異なりますが遷移タイプがありません — 値は寿命全体で開始値のままです。"),
 ("VNoVisual","neither shader nor model — the primitive stays invisible.","weder Shader noch Modell — die Primitive bleibt unsichtbar.","既无着色器也无模型 — 该基元将不可见。","シェーダーもモデルもありません — このプリミティブは表示されません。"),
 ("VSoundNoFile","Sound without a sound file.","Sound ohne Klangdatei.","Sound 没有声音文件。","Sound にサウンドファイルがありません。"),
 ("VRunnerNoFx","FxRunner without a playfx entry.","FxRunner ohne playfx-Eintrag.","FxRunner 没有 playfx 条目。","FxRunner に playfx がありません。"),
 ("VNoLife","no life set. The engine then uses its default of 50 ms. Intended for effects that spawn continuously.","kein life gesetzt. Die Engine nimmt dann ihre Voreinstellung von 50 ms. Gewollt bei Effekten, die fortlaufend neu ausgelöst werden.","未设置 life。引擎将使用默认值 50 毫秒。对持续生成的特效而言是有意为之。","life が未設定です。エンジンは既定値の 50 ミリ秒を使います。継続生成するエフェクトでは意図的です。"),
 ("VShakeNoBounce","CameraShake without bounce. bounce is the strength; without it the shake is 0 and does nothing in game.","CameraShake ohne bounce. bounce ist die Stärke; ohne sie ist der Rüttler 0 und im Spiel wirkungslos.","CameraShake 没有 bounce。bounce 即强度；没有它震动为 0，在游戏中无效。","CameraShake に bounce がありません。bounce は強さで、無いと 0 になりゲーム中で何も起きません。"),
 ("VShakeNoRadius","CameraShake without radius. Outside the radius nothing happens, and the engine's default is only 10 units.","CameraShake ohne radius. Außerhalb des Radius passiert nichts, und die Voreinstellung der Engine ist nur 10 Einheiten.","CameraShake 没有 radius。半径之外不会发生任何事，而引擎的默认值只有 10 个单位。","CameraShake に radius がありません。半径の外では何も起きず、エンジンの既定値はわずか 10 ユニットです。"),
 ("VShakeTooStrong","bounce above 16. CGCam_Shake caps at MAX_SHAKE_INTENSITY = 16; higher values have no effect.","bounce über 16. CGCam_Shake begrenzt auf MAX_SHAKE_INTENSITY = 16, höhere Werte wirken nicht.","bounce 超过 16。CGCam_Shake 限制在 MAX_SHAKE_INTENSITY = 16，更高的值无效。","bounce が16を超えています。CGCam_Shake は MAX_SHAKE_INTENSITY = 16 で制限し、それ以上は効きません。"),
 ("VShakeIgnored","shaders or sounds on a CameraShake are ignored.","Shader oder Klänge an einem CameraShake werden ignoriert.","CameraShake 上的着色器或声音会被忽略。","CameraShake のシェーダーやサウンドは無視されます。"),
 ("VCurveCollision","has %s. nonlinear, wave and clamp share two bits — the engine reads exactly one kind from them.","hat %s. nonlinear, wave und clamp teilen sich zwei Bits — die Engine liest daraus genau eine Art.","具有 %s。nonlinear、wave 和 clamp 共用两个位 — 引擎只会读出其中一种。","%s があります。nonlinear・wave・clamp は2ビットを共有し、エンジンは1種類だけを読み取ります。"),
 ("VCurveNoValues","has a transition but neither start nor end.","hat eine Übergangsart, aber weder start noch end.","设置了过渡方式，但既无 start 也无 end。","トランジションはありますが start も end もありません。"),
 ("VWaveNoEnd","is set to wave without end. The engine's end is then 1.0: the value swings between start and 1.","ist auf wave gesetzt, ohne end. Das Ende ist dann 1.0: der Wert schwingt zwischen start und 1.","设为 wave 但没有 end。引擎的结束值此时为 1.0：数值在 start 与 1 之间摆动。","end の無い wave です。エンジンの終了値は 1.0 となり、値は start と 1 の間で振動します。"),
 ("VRandomNoEnd","is random without end. The engine's end is then 1.0: the value varies between start and 1, not around start.","ist random ohne end. Das Ende ist dann 1.0: der Wert schwankt zwischen start und 1, nicht um start herum.","设为 random 但没有 end。引擎的结束值此时为 1.0：数值在 start 与 1 之间变化，而非围绕 start。","end の無い random です。エンジンの終了値は 1.0 となり、値は start と 1 の間で変動し、start のまわりではありません。"),
 ("VWindDead","wind has no effect in any game mode. Singleplayer does not know affectedByWind, and in multiplayer the evaluating block is commented out (CL_GetWindVector was never written).","Wind wirkt in keinem Spielmodus. Der Singleplayer kennt affectedByWind nicht, und im Multiplayer ist der auswertende Block auskommentiert (CL_GetWindVector wurde nie geschrieben).","风在任何游戏模式下都无效。单人模式不识别 affectedByWind，多人模式中相关代码被注释掉（CL_GetWindVector 从未实现）。","風はどのモードでも効きません。シングルでは affectedByWind を読まず、マルチでは評価部分がコメントアウトされています（CL_GetWindVector は未実装）。"),
 ("VEndNoCurve","has a differing end but no transition — the value stays at its start value for the whole lifetime.","hat ein abweichendes Ende, aber keine Übergangsart — der Wert bleibt die ganze Lebensdauer auf dem Startwert.","end 与 start 不同但没有过渡方式 — 数值在整个生命周期内保持起始值。","end が異なりますがトランジションがありません — 値はライフ中ずっと開始値のままです。"),
 ("VRgbEndNoCurve","rgb has a differing end colour but no transition — the start colour stays.","rgb hat eine abweichende Endfarbe, aber keine Übergangsart — die Startfarbe bleibt stehen.","rgb 的结束颜色不同但没有过渡方式 — 起始颜色保持不变。","rgb の終了色が異なりますがトランジションがありません — 開始色のままです。"),
 # Zwei Varianten derselben Sache: schreibt man fuer beide Zweige, wird
 # das Flag im anderen stillschweigend ueberlesen; schreibt man fuer
 # einen, ist es schlicht der falsche. Die Reihenfolge der Werte ist in
 # allen vier Sprachen dieselbe — Art, Name, Zweig (, Ziel).
 ("VFlagOnlyIn","%s \"%s\" only exists in the %s branch; the other reads over it silently.","%s \"%s\" gibt es nur im %s-Zweig, im anderen wird es stumm überlesen.","%s \"%s\" 仅存在于 %s 分支，另一分支会静默忽略。","%s \"%s\" は %s 側にのみ存在し、もう一方では黙って読み飛ばされます。"),
 ("VFlagWrongBranch","%s \"%s\" only exists in the %s branch, but the target is %s.","%s \"%s\" gibt es nur im %s-Zweig, das Ziel ist %s.","%s \"%s\" 仅存在于 %s 分支，但目标是 %s。","%s \"%s\" は %s 側にのみ存在しますが、対象は %s です。"),
 ("VLessAttenuation","lessAttenuation only affects Sound.","lessAttenuation wirkt nur auf Sound.","lessAttenuation 仅对 Sound 有效。","lessAttenuation は Sound にのみ効きます。"),
 ("VMaterialImpact","materialImpact is only known to the multiplayer parser.","materialImpact kennt nur der Multiplayer-Parser.","materialImpact 仅多人模式解析器识别。","materialImpact はマルチプレイヤーのパーサーのみが認識します。"),
 ("VSize2Collision","paperPhysics, localizedFlash and playerView occupy the same bit as the size2 curve. On a cylinder that changes the size2 curve.","paperPhysics, localizedFlash und playerView belegen dasselbe Bit wie die size2-Kurve. An einem Cylinder verändert das die size2-Kurve.","paperPhysics、localizedFlash 和 playerView 占用与 size2 曲线相同的位。在 Cylinder 上会改变 size2 曲线。","paperPhysics・localizedFlash・playerView は size2 カーブと同じビットを使います。Cylinder では size2 カーブが変わります。"),
 ("VFlagWrongType","paperPhysics only applies to Emitter, localizedFlash only to Flash, playerView only to view effects.","paperPhysics gilt nur für Emitter, localizedFlash nur für Flash, playerView nur für Ansichtseffekte.","paperPhysics 仅适用于 Emitter，localizedFlash 仅适用于 Flash，playerView 仅适用于视图特效。","paperPhysics は Emitter、localizedFlash は Flash、playerView はビューエフェクト専用です。"),
 ("VGhoul2Collision","ghoul2Collision and ghoul2Decals occupy the same bits as the size2 curve. On a cylinder that yields a wrong curve.","ghoul2Collision und ghoul2Decals belegen dieselben Bits wie die size2-Kurve. An einem Cylinder ergibt das eine falsche Kurve.","ghoul2Collision 和 ghoul2Decals 占用与 size2 曲线相同的位。在 Cylinder 上会产生错误的曲线。","ghoul2Collision と ghoul2Decals は size2 カーブと同じビットを使います。Cylinder では誤ったカーブになります。"),
 ("VEmitFxEmpty","emitFx set but no emitfx list.","emitFx gesetzt, aber keine emitfx-Liste.","设置了 emitFx 但没有 emitfx 列表。","emitFx が設定されていますが emitfx のリストがありません。"),
 ("VImpactFxEmpty","impactFx set but no impactfx list.","impactFx gesetzt, aber keine impactfx-Liste.","设置了 impactFx 但没有 impactfx 列表。","impactFx が設定されていますが impactfx のリストがありません。"),
 ("VDeathFxEmpty","deathFx set but no deathfx list.","deathFx gesetzt, aber keine deathfx-Liste.","设置了 deathFx 但没有 deathfx 列表。","deathFx が設定されていますが deathfx のリストがありません。"),
 ("VImpactKills","impactKills only works together with usePhysics.","impactKills wirkt nur zusammen mit usePhysics.","impactKills 仅在配合 usePhysics 时有效。","impactKills は usePhysics と併用したときのみ効きます。"),
 ("VModelEmpty","useModel set but no models list.","useModel gesetzt, aber keine models-Liste.","设置了 useModel 但没有 models 列表。","useModel が設定されていますが models のリストがありません。"),
 ("VElectricityFlags","on Electricity these flags mean something else —%s. That is intended, not accidental.","bei Electricity bedeuten diese Flags etwas anderes —%s. Das ist beabsichtigt, nicht versehentlich.","在 Electricity 上这些标志含义不同 —%s。这是有意为之，而非误设。","Electricity ではこれらのフラグは別の意味です —%s。意図的なもので、間違いではありません。"),
 # --- Ungespeicherte Aenderungen, Feldbearbeitung (Nacht 1./2.10.2026)
 ("UndoFieldChange","field changed","Feld geändert","字段已更改","フィールド変更"),
 ("UndoSortSegments","segments sorted","Segmente sortiert","片段已排序","セグメントを並べ替え"),
 # --- Eigenschaftsseite wie im Original (Nacht 1./2.10.2026). Wortlaut
 #     englisch aus den Dialogressourcen von EffectsEd.exe.
 ("FieldDelayMs","Delay (ms)","Verzögerung (ms)","延迟 (毫秒)","遅延 (ms)"),
 ("FieldLifeMs","Life (ms)","Lebensdauer (ms)","生命周期 (毫秒)","寿命 (ms)"),
 ("GenUseCulling","Use distance culling","Ab Entfernung ausblenden","使用距离剔除","距離で非表示にする"),
 ("FieldCullDistance","Cull Distance","Ausblendentfernung","剔除距离","非表示距離"),
 ("DeathEnable","Enable Death Effects","Ende-Effekte einschalten","启用消亡特效","消滅エフェクトを有効にする"),
 ("OriginEnableSpecial","Enable special offset types","Besondere Verteilung einschalten","启用特殊偏移类型","特殊オフセットを有効にする"),
 ("FieldRadiusWidth","Radius/Width","Radius/Breite","半径/宽度","半径/幅"),
 ("GroupSizeWidth","Size/Width","Größe/Breite","大小/宽度","サイズ/幅"),
 ("FieldStartSize","Start Size","Anfangsgröße","起始大小","開始サイズ"),
 ("FieldEndSize","End Size","Endgröße","结束大小","終了サイズ"),
 ("CurveApplyRandom","Apply random factor","Zufallsfaktor anwenden","应用随机因子","ランダム係数を適用"),
 ("TransConstant","Constant","Konstant","恒定","一定"),
 ("TransLinear","Linear","Linear","线性","線形"),
 ("TransNonlinear","Nonlinear","Nichtlinear","非线性","非線形"),
 ("TransNonlinearLinear","Nonlinear/Linear","Nichtlinear/Linear","非线性/线性","非線形/線形"),
 ("TransWave","Wave","Welle","波形","波"),
 ("TransWaveLinear","Wave/Linear","Welle/Linear","波形/线性","波/線形"),
 ("TransClamp","Clamp","Klemmen","钳制","クランプ"),
 ("TransClampLinear","Clamp/Linear","Klemmen/Linear","钳制/线性","クランプ/線形"),
 ("FieldStartLength","Start Length","Anfangslänge","起始长度","開始長さ"),
 ("FieldEndLength","End Length","Endlänge","结束长度","終了長さ"),
 ("GroupSize2Width2","Size2/Width2","Größe 2/Breite 2","大小2/宽度2","サイズ2/幅2"),
 ("FieldStartSize2","Start Size2","Anfangsgröße 2","起始大小2","開始サイズ2"),
 ("FieldEndSize2","End Size2","Endgröße 2","结束大小2","終了サイズ2"),
 ("GroupRgbColor","RGB Color","RGB-Farbe","RGB 颜色","RGB カラー"),
 ("FieldStartColor","Start Color","Anfangsfarbe","起始颜色","開始色"),
 ("FieldEndColor","End Color","Endfarbe","结束颜色","終了色"),
 ("ColorPickCube","Pick start/end in color cube","Anfang/Ende im Farbwürfel wählen","在颜色立方体中选取起止颜色","カラーキューブで開始/終了を選ぶ"),
 ("GroupAlphaTransparency","Alpha Transparency","Alpha-Transparenz","透明度","アルファ透明度"),
 ("FieldStartAlpha","Start Alpha","Anfangs-Alpha","起始透明度","開始アルファ"),
 ("FieldEndAlpha","End Alpha","End-Alpha","结束透明度","終了アルファ"),
 ("PhysicsEnable","Enable Physics","Physik einschalten","启用物理","物理を有効にする"),
 ("PhysicsEnableBBox","Enable physics bounding box","Begrenzungsbox einschalten","启用物理包围盒","物理バウンディングボックスを有効にする"),
 ("FieldBounceOnly","Bounce","Rückprall","弹跳","跳ね返り"),
 ("LineEndpoint","Endpoint","Endpunkt","终点","終点"),
 ("MotionPercentage","Percentage","Anteil","百分比","割合"),
 ("MotionPercentHint","( generally 1 to 100 )","( üblich 1 bis 100 )","( 通常 1 到 100 )","( 通常 1〜100 )"),
 ("GroupAngle","Angle","Winkel","角度","角度"),
 ("GroupAngleDelta","Angle Delta","Winkeländerung","角度增量","角度変化"),
 ("GroupSpawnedEffects","Spawned Effects","Gestartete Effekte","生成的特效","生成エフェクト"),
 ("PropAdvancedFlags","Advanced: all flags","Erweitert: alle Flags","高级：全部标志","詳細：すべてのフラグ"),
 ("PropDefaultHint","Grey values are engine defaults and are not written to the file","Graue Werte sind Vorgaben der Engine und stehen nicht in der Datei","灰色数值是引擎默认值，不会写入文件","灰色の値はエンジンの既定値で、ファイルには書き込まれません"),
 ("ListRename","Rename","Umbenennen","重命名","名前を変更"),
 ("AboutOriginal","Modelled on Raven Software's EffectsEd 1.1 (2001-2003) by Dave Blumenthal, Jeff Dischler, Aurelio Reis, Ste Cork and Mike Crowns.","Nachgebaut nach Raven Softwares EffectsEd 1.1 (2001-2003) von Dave Blumenthal, Jeff Dischler, Aurelio Reis, Ste Cork und Mike Crowns.","仿照 Raven Software 的 EffectsEd 1.1（2001-2003），原作者 Dave Blumenthal、Jeff Dischler、Aurelio Reis、Ste Cork、Mike Crowns。","Raven Software の EffectsEd 1.1（2001-2003、Dave Blumenthal、Jeff Dischler、Aurelio Reis、Ste Cork、Mike Crowns）を基に再構築。"),
 # --- Hilfezeile (Statusleiste), Wortlaut der String-Tabelle des Originals
 ("HintFileNew","Create a new document","Neues Dokument anlegen","新建文档","新しいドキュメントを作成"),
 ("HintFileOpen","Open an existing document","Vorhandenes Dokument öffnen","打开现有文档","既存のドキュメントを開く"),
 ("HintFileSave","Save the active document","Aktives Dokument speichern","保存当前文档","アクティブなドキュメントを保存"),
 ("HintFileSaveAs","Save the active document with a new name","Aktives Dokument unter neuem Namen speichern","以新名称保存当前文档","名前を付けて保存"),
 ("HintReloadAssets","Reloads textures, shaders, and effect files that are used by the current effect","Lädt Texturen, Shader und Effekte neu, die der aktuelle Effekt benutzt","重新加载当前特效使用的纹理、着色器和特效文件","現在のエフェクトが使うテクスチャ、シェーダー、エフェクトを再読み込み"),
 ("HintFileExit","Quit the application; prompts to save documents","Programm beenden; fragt nach dem Speichern","退出程序；会提示保存文档","アプリを終了します。保存を確認します"),
 ("HintClone","Clones the selected effect segment.","Klont das gewählte Segment.","克隆所选片段。","選択したセグメントを複製します。"),
 ("HintDelete","Delete the selected effect segment","Gewähltes Segment löschen","删除所选片段","選択したセグメントを削除"),
 ("HintWallColor","Set the color used for the walls and floor of the testing room","Farbe für Wände und Boden des Testraums","设置测试房间墙壁和地面的颜色","テストルームの壁と床の色を設定"),
 ("HintBgColor","Set the background color of the world view","Hintergrundfarbe der 3D-Ansicht","设置视图背景颜色","ビューの背景色を設定"),
 ("HintGamePath","Select the default game path used for new effects","Spielpfad für neue Effekte festlegen","选择新特效使用的默认游戏路径","新しいエフェクトで使う既定のゲームパスを選択"),
 ("HintDrawAxes","Draw world orientation axes","Achsenkreuz der Welt zeichnen","绘制世界坐标轴","ワールド軸を表示"),
 ("HintWind","Draw global wind vector","Globalen Windvektor zeichnen","绘制全局风向量","全体の風ベクトルを表示"),
 ("HintDrawRoom","Draw the walls of the testing room","Wände des Testraums zeichnen","绘制测试房间的墙壁","テストルームの壁を表示"),
 ("HintDrawGrid","Draw outline of testing room","Umriss des Testraums zeichnen","绘制测试房间轮廓","テストルームの輪郭を表示"),
 ("HintResetView","Used for when you've moved/zoomed your object offscreen and can't find it.","Wenn der Effekt aus dem Bild gedreht oder gezoomt ist und nicht mehr zu finden ist.","当物体被移出或缩放到屏幕外找不到时使用。","オブジェクトが画面外に出て見つからないときに使います。"),
 ("HintScreenshot","Takes a screenshot and saves it to a file","Bildschirmfoto der Ansicht in eine Datei","截图并保存到文件","スクリーンショットをファイルに保存"),
 ("HintScreenshotClip","Takes a screenshot and sends it to the clipboard","Bildschirmfoto der Ansicht in die Zwischenablage","截图并复制到剪贴板","スクリーンショットをクリップボードへ"),
 ("HintGraphicsInfo","View whatever the graphics driver sends back when interrogated","Zeigt, was der Grafiktreiber über sich meldet","查看显卡驱动返回的信息","グラフィックドライバーの情報を表示"),
 ("HintNewSegment","Create a new effect segment","Neues Segment anlegen","创建新的特效片段","新しいエフェクトセグメントを作成"),
 ("HintEnabled","Enable or disable selected effect segment","Gewähltes Segment ein- oder ausschalten","启用或禁用所选片段","選択したセグメントを有効/無効にする"),
 ("HintPlay","Play effect","Effekt abspielen","播放特效","エフェクトを再生"),
 ("HintPause","Pause time","Zeit anhalten","暂停时间","時間を一時停止"),
 ("HintStop","Stop effects","Effekte beenden","停止特效","エフェクトを停止"),
 ("HintPlaybackSettings","Displays the Playback Settings dialog box","Öffnet die Wiedergabe-Einstellungen","显示播放设置对话框","再生設定ダイアログを表示"),
 ("HintOrientUp","Orient effect up (on Z axis)","Effekt nach oben ausrichten (Z-Achse)","特效朝上（Z 轴）","エフェクトを上向きに（Z 軸）"),
 ("HintOrientSide","Orient effect sideways (on X axis)","Effekt seitwärts ausrichten (X-Achse)","特效朝侧面（X 轴）","エフェクトを横向きに（X 軸）"),
 ("HintOrientDown","Orient effect down (on Z axis)","Effekt nach unten ausrichten (Z-Achse)","特效朝下（Z 轴）","エフェクトを下向きに（Z 軸）"),
 ("HintCustomOrigin","Set custom FX spawn origin","Eigenen Startpunkt des Effekts festlegen","设置自定义特效生成原点","エフェクトの発生位置を設定"),
 ("HintPlaySounds","Turns any kind of sound play back on or off","Schaltet die Klangwiedergabe ein oder aus","打开或关闭声音播放","サウンド再生のオン/オフ"),
 ("HintAbout","Display program information, version number and copyright","Programminformation, Fassung und Urheberrecht","显示程序信息、版本号和版权","プログラム情報、バージョン、著作権を表示"),
 ("MsgYes","Yes","Ja","是","はい"),
 ("MsgNo","No","Nein","否","いいえ"),
 ("PhysicsExpensiveWarning","This physics type should not be used unless absolutely necessary since it will make any effect significantly more expensive. Would you like to continue and use this physics type anyway?","Diese Physik sollte nur benutzt werden, wenn es unbedingt nötig ist — sie macht jeden Effekt deutlich teurer. Trotzdem benutzen?","除非绝对必要，否则不应使用此物理类型，因为它会显著增加任何特效的开销。仍要继续使用吗？","この物理タイプは絶対に必要な場合以外は使用しないでください。エフェクトの負荷が大幅に増加します。それでも使用しますか？"),
 ("ListUnnamedPrefix","Unnamed","Unbenannt","未命名","名前なし"),
 ("ListCopyOf","Copy of","Kopie von","副本：","コピー："),
 ("BoxX","X","X","X","X"),
 ("BoxY","Y","Y","Y","Y"),
 ("BoxZ","Z","Z","Z","Z"),
 ("SaveChangesTitle","Save Changes","Änderungen speichern","保存更改","変更を保存"),
 ("SaveChangesText","Save changes to %s?","Änderungen an %s speichern?","是否保存对 %s 的更改？","%s への変更を保存しますか？"),
 ("SaveChangesYes","Save","Speichern","保存","保存"),
 ("SaveChangesNo","Don't Save","Nicht speichern","不保存","保存しない"),
 ("MsgScreenshotClipboard","Screenshot copied to clipboard","Bildschirmfoto in der Zwischenablage","截图已复制到剪贴板","スクリーンショットをクリップボードにコピーしました"),
 # --- Auto-Updater (gui/update_win32.cpp) ---
 ("HelpCheckUpdates","Check for Updates...","Nach Updates suchen...","检查更新...","更新を確認..."),
 ("HelpUpdateOnStart","Check for Updates at Startup","Beim Start nach Updates suchen","启动时检查更新","起動時に更新を確認"),
 ("UpdTitle","Update","Update","更新","更新"),
 ("UpdInstalled","Installed: %s","Installiert: %s","已安装：%s","インストール済み：%s"),
 ("UpdChecking","Asking GitHub for a newer version...","Frage GitHub nach einer neueren Fassung...","正在向 GitHub 查询新版本...","GitHub で新しいバージョンを確認しています..."),
 ("UpdCurrent","This is the newest version.","Das ist die neueste Fassung.","已是最新版本。","最新バージョンです。"),
 ("UpdAvailable","New version available: %s","Neue Fassung verfügbar: %s","有新版本：%s","新しいバージョンがあります：%s"),
 ("UpdNotes","What's new:","Neu darin:","更新内容：","変更点："),
 ("UpdDone","Installed (%d files). Restart to use the new version.","Installiert (%d Dateien). Nach dem Neustart läuft die neue Fassung.","已安装（%d 个文件）。重新启动后使用新版本。","インストールしました（%d ファイル）。再起動すると新しいバージョンになります。"),
 ("UpdError","Update failed: %s","Update fehlgeschlagen: %s","更新失败：%s","更新に失敗しました：%s"),
 ("UpdInstall","Download and Install","Herunterladen und installieren","下载并安装","ダウンロードしてインストール"),
 ("UpdRestart","Restart Now","Jetzt neu starten","立即重新启动","今すぐ再起動"),
 ("UpdRetry","Check Again","Erneut prüfen","重新检查","再確認"),
 ("UpdOpenPage","Open Release Page","Release-Seite öffnen","打开发布页面","リリースページを開く"),
 ("UpdLater","Later","Später","稍后","後で"),
 ("UpdStatusAvail","Update %s available","Update %s verfügbar","有可用更新 %s","更新 %s があります"),
 ("UpdStatusDone","Update installed - restart to use it","Update installiert - Neustart nötig","更新已安装 - 需要重新启动","更新をインストールしました - 再起動が必要です"),
 ("UpdNoRelease","There is no release on GitHub yet.","Auf GitHub gibt es noch kein Release.","GitHub 上还没有发布版本。","GitHub にはまだリリースがありません。"),
 ("UpdNoAccess","GitHub refused the request (too many requests from this network?). Try again later.","GitHub hat die Anfrage abgelehnt (zu viele Anfragen aus diesem Netz?). Später erneut versuchen.","GitHub 拒绝了请求（此网络请求过多？）。请稍后再试。","GitHub がリクエストを拒否しました（このネットワークからのリクエストが多すぎる？）。後でもう一度お試しください。"),
 ("UpdNoZip","The release contains no .zip file.","Das Release enthält keine .zip-Datei.","该发布版本不包含 .zip 文件。","リリースに .zip ファイルがありません。"),
 ("UpdErrConnect","No connection to %s.","Keine Verbindung zu %s.","无法连接到 %s。","%s に接続できません。"),
 ("UpdErrNoAnswer","%s did not answer (error %lu).","%s antwortet nicht (Fehler %lu).","%s 没有响应（错误 %lu）。","%s から応答がありません（エラー %lu）。"),
 ("UpdErrBadAnswer","Unexpected answer from GitHub (HTTP %d): %s","Unerwartete Antwort von GitHub (HTTP %d): %s","GitHub 返回了意外的响应（HTTP %d）：%s","GitHub から予期しない応答がありました（HTTP %d）：%s"),
 ("UpdErrDownload","Download failed: %s","Herunterladen fehlgeschlagen: %s","下载失败：%s","ダウンロードに失敗しました：%s"),
 ("UpdErrZip","The download is not a readable .zip: %s","Der Download ist kein lesbares .zip: %s","下载的文件不是可读取的 .zip：%s","ダウンロードしたファイルは読み取れる .zip ではありません：%s"),
 ("UpdErrWrite","Could not write %s. Is the program folder write-protected (e.g. under Program Files)?","%s ließ sich nicht schreiben. Ist der Programmordner schreibgeschützt (etwa unter Programme)?","无法写入 %s。程序文件夹是否受写保护（例如位于 Program Files 下）？","%s を書き込めませんでした。プログラムフォルダーが書き込み禁止になっていませんか（Program Files の下など）？"),
 ("UpdErrReplace","Could not replace %s (error %lu).","%s ließ sich nicht ersetzen (Fehler %lu).","无法替换 %s（错误 %lu）。","%s を置き換えられませんでした（エラー %lu）。"),
]

# --- Formatplatzhalter pruefen -------------------------------------------
#
# Die Meldungen der Pruefung werden mit snprintf zusammengesetzt, und die
# Formatzeichenkette kommt aus DIESER Tabelle. Der Uebersetzer kann sie
# deshalb nicht pruefen (-Wformat-nonliteral sagt das auch).
#
# Schreibt jemand in einer Sprache %d, wo die anderen %s haben, ist das
# undefiniertes Verhalten — in der Regel ein Absturz, und nur in dieser einen
# Sprache. Also hier pruefen, wo es billig ist.
import re as _re
_SPEC = _re.compile(r'%[-+ #0]*[0-9]*(?:\.[0-9]+)?[hlLqjzt]*([diouxXeEfgGaAcspn%])')
_problems = []
for _entry in S:
    _name, _variants = _entry[0], _entry[1:]
    _specs = [tuple(m for m in _SPEC.findall(v) if m != '%') for v in _variants]
    if len(set(_specs)) > 1:
        _problems.append((_name, _specs))
if _problems:
    for _name, _specs in _problems:
        print(f"FEHLER: {_name} hat unterschiedliche Platzhalter: {_specs}")
    raise SystemExit("Platzhalter stimmen zwischen den Sprachen nicht ueberein")

names = [s[0] for s in S]
assert len(names) == len(set(names)), "doppelte Kennung"

h = '''// Oberflächentexte in vier Sprachen.
//
// Erzeugt von tools/gen_i18n.py — nicht von Hand ändern, sondern dort.
// Der Aufbau folgt g2c: ein Aufzählungswert je Text, eine Tabelle mit vier
// Spalten, und ein static_assert, das Tabellenlänge und Aufzählung vergleicht.
// Geraten beide aus dem Tritt, fällt es beim Übersetzen auf und nicht als
// leerer Knopf im laufenden Programm.
//
// Hauptsprache ist Englisch: das ist die Sprache der JKA-Modding-Gemeinde,
// der Dateiformate und der Fehlermeldungen der Engine.
#pragma once

#include <string>
#include <vector>

namespace efx::i18n {

enum class Language {
    English,
    German,
    ChineseSimplified,
    Japanese,
};

enum class Str {
'''
for n in names:
    h += "    %s,\n" % n
h += '''    Count,
};

// Der aktuelle Text. Kurzform tr() für den Gebrauch im Code.
const char* tr(Str id);

Language currentLanguage();
void setLanguage(Language language);

struct LanguageInfo {
    Language language;
    const char* code;       // für die Einstellungsdatei: en, de, zh-Hans, ja
    const char* nativeName; // im Menü, in der eigenen Schrift
    bool needsCjkFont;      // ob eine zusätzliche Schrift geladen werden muss
};
const std::vector<LanguageInfo>& languages();
const LanguageInfo* findLanguage(const std::string& code);

// Sprache aus den Windows-Regionseinstellungen ableiten. Der Aufrufer gibt
// den Sprachcode des Systems herein (etwa "de-DE"); getrennt gehalten, damit
// die Zuordnung ohne Windows prüfbar bleibt.
Language fromSystemLocale(const std::string& locale);

// Nur für Tests: liefert einen Text in einer bestimmten Sprache.
const char* trIn(Language language, Str id);

}  // namespace efx::i18n
'''
open(_ROOT + '/include/efx/i18n.h','w',encoding='utf-8',newline='\n').write(h)

def esc(s):
    return s.replace('\\','\\\\').replace('"','\\"')

c = '''// Erzeugt von tools/gen_i18n.py — nicht von Hand ändern.
#include "efx/i18n.h"

namespace efx::i18n {
namespace {

struct Entry {
    const char* en;
    const char* de;
    const char* zh;
    const char* ja;
};

// Reihenfolge muss der Aufzählung Str entsprechen.
const Entry kTable[] = {
'''
for (n,en,de,zh,ja) in S:
    c += '    {"%s", "%s", "%s", "%s"},  // %s\n' % (esc(en),esc(de),esc(zh),esc(ja),n)
c += '''};

static_assert(sizeof(kTable) / sizeof(kTable[0]) == static_cast<size_t>(Str::Count),
              "Die Texttabelle passt nicht zur Aufzählung Str. "
              "Beide werden von tools/gen_i18n.py erzeugt — dort ändern.");

Language g_language = Language::English;

}  // namespace

const char* trIn(Language language, Str id) {
    size_t index = static_cast<size_t>(id);
    if (index >= static_cast<size_t>(Str::Count)) return "";
    const Entry& e = kTable[index];
    switch (language) {
        case Language::German: return e.de;
        case Language::ChineseSimplified: return e.zh;
        case Language::Japanese: return e.ja;
        default: return e.en;
    }
}

const char* tr(Str id) { return trIn(g_language, id); }

Language currentLanguage() { return g_language; }
void setLanguage(Language language) { g_language = language; }

const std::vector<LanguageInfo>& languages() {
    static const std::vector<LanguageInfo> kLanguages = {
        {Language::English, "en", "English", false},
        {Language::German, "de", "Deutsch", false},
        {Language::ChineseSimplified, "zh-Hans", "\\u4e2d\\u6587", true},
        {Language::Japanese, "ja", "\\u65e5\\u672c\\u8a9e", true},
    };
    return kLanguages;
}

const LanguageInfo* findLanguage(const std::string& code) {
    for (const auto& l : languages()) {
        if (code == l.code) return &l;
    }
    return nullptr;
}

Language fromSystemLocale(const std::string& locale) {
    // Nur das Präfix vergleichen: "de-DE", "de-AT" und "de" sind alle Deutsch.
    // Chinesisch braucht mehr Sorgfalt — zh-Hant (Taiwan, Hongkong) ist nicht
    // dasselbe wie zh-Hans, und wir haben nur Vereinfachtes. Für zh-Hant
    // bleibt Englisch die bessere Wahl als falsche Zeichen.
    auto startsWith = [&](const char* prefix) {
        size_t n = 0;
        while (prefix[n]) ++n;
        return locale.size() >= n && locale.compare(0, n, prefix) == 0;
    };

    if (startsWith("de")) return Language::German;
    if (startsWith("ja")) return Language::Japanese;
    if (startsWith("zh-Hant") || startsWith("zh-TW") || startsWith("zh-HK") ||
        startsWith("zh-MO")) {
        return Language::English;
    }
    if (startsWith("zh")) return Language::ChineseSimplified;
    return Language::English;
}

}  // namespace efx::i18n
'''
open(_ROOT + '/src/i18n.cpp','w',encoding='utf-8',newline='\n').write(c)
print("i18n erzeugt:", len(S), "Texte x 4 Sprachen =", len(S)*4)

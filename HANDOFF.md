# 引き継ぎメモ（最新: 2026-11-28 17 時台（PC の時計）。再開はこの冒頭の「再開の手順」から）

## 再開の手順（2026-11-29 1 時、(4) プレビューに反映されない件の原因が判明。利用枠 95% で中断）

- **利用者の回答:** ドラッグ中も離した後も、エディターの変更はすべてプレビューに反映されず、Effect Controls で Macro を動かすまで出ない。
- **再現（新しい試験 `pwsh tests/ae/run_dx2_live.ps1`、`dx2_live.jsx`・`dx2_live_script.txt`）:** コンポをビューアーで開き、Idle Hook でグラフを
  書き換える（seed 5 の台本 `set g countX 3`）→ ビューアーが描き直さず、キャッシュを捨てずに描いても古い絵（25 個）。Macro を変えると 15 個。
- **原因（試して確かめた）:** AE は Arbitrary Data を AEGP で書き換えただけでは描き直さない。数のパラメータが変われば描き直す。
  ただし **AEGP の HIDDEN フラグを付けたパラメータの変化は無視される**（隠した「Graph Revision」を書いても描き直さず、隠さないと描き直した）。
- **入れたもの（commit 済み、作業中）:** Disk ID 103 に「Graph Revision」（`P_REVISION`、`dx2_params.h`・`dx2.cpp`）。書き込み（`dx2_editor.cpp` の `DoWrite`）で
  毎回 +1、PreRender で読む。**今は実験として隠していない**（`dx2_aegp.cpp` の `// EXPERIMENT want[P_REVISION] = 1;`）→ Effect Controls に行が見える。
- **次の 1 手:** 行を見せずに効かせる方法を試す: (a) AEGP の HIDDEN は付けず `PF_PUI_INVISIBLE`（UI だけ）で隠す、(b) だめなら別の手。
  `run_dx2_live.ps1` で DX2 LIVE PASS を確かめ、`run_dx2_graph.ps1` 8・32 bpc と `run_dx2_p1_fixture.ps1`（パラメータが 104 個に増えた。P1 の aep が開くか）も。
  Graph Revision は CANNOT_TIME_VARY にするか検討（キーを打たれないように）。
- **(10) の Math の並び**は直したが、利用者に見てもらう。

## 再開の手順（2026-11-29、利用者の試用の指摘 12 件に対応中）

- **利用者の指摘（試用、2026-11-29）:** (1) unused の展開を連打できない（クールタイムをなくす）。(2) ノードの削除を X キーで、右クリックのメニューにも。
  (3) **Fast (2D) なのに Copy Scale を変えると非常に重い**、広げると重くなるなど謎が多い。DupliX（版 1）の高速化モードと速さが違いすぎる。
  **版 1 の高速化モードの実装をちゃんと見る。推測で実装しない。** (4) **エディターでの変更がプレビューにリアルタイムで反映されない。**
  (5) 左ドラッグで範囲選択。(6) 範囲選択してドラッグで全部動く（今はクリックしたノードだけ）。(7) Effect Controls のエラー文が見づらい・見切れる、
  Effect Controls に出る型の誤りがエディターに出ないのも分かりづらい。(8) Float の入力で 100.0 ちょうどだと小数点が省略される。
  (9) Float のドラッグが細かすぎる: 普通 1.0、Shift 10 倍、Ctrl 1/10。Int は 1・10・1（ゆっくり）。(10) Math ノードのレイアウトが崩れて見える、揃える。
  (11) 右クリックのメニューと演算子の一覧を自前のメニューに。Ctrl+A でもノード追加メニュー、そのまま打つと検索。
  (12) Rotate の rot_apply で a に Int Macro を付けられない（b は付く）。理由があっても見分けられず、エラーが Effect Controls にしか出ない。
- **済み:** (3) `4f30534`: 版 1 の高速化モード（`src/core/raster.cpp`）と同じ描き方に: 1 画素 1 回の補間（縮小で n×n 回やめ）、
  手前のコピーから「下に重ねる」で描き不透明な画素は飛ばす。`tests/ae/dx2_perf_compare.jsx`（版 1 と同じ 25 個を並べて測る）で
  Copy Scale 25%: 1013 → 30 ms、300%: 1150〜1943 → 71〜112 ms、100%: 135 → 63 ms。GPU を空けて版 1（GPU）と比べても 1 フレームの時間は同程度。
  **測定の注意:** 利用者「別プロセスが GPU を使っていた」→ 比べ直しは `nvidia-smi` で空きを確かめてから（使用率 12%・3.4/12 GB）。
  (1)(2)(5)(6)(7)(8)(9)(10)(11)(12) `fd7c637`: 自前のメニュー（打つと絞り込み、Ctrl+A）、X / Delete と右クリックの Delete、範囲選択とまとめて移動、
  unused の 2 回目の素早いクリック、Float は 100.0、ドラッグは 1 画素 1（Shift 10、Ctrl 0.1。Int の Ctrl は 10 画素で 1）、Math の op の行を値の列に揃え
  入力に型を表示、型の合わないマクロや線はつなぐときに理由つきで拒む（rot_apply の a は Rotation）、描画の誤りをエディターに表示（Overview は全幅＋件数）。
  試験: `dxg5_test` ALL PASS、AE 8・32 bpc DX2 GRAPH PASS。
- **利用者に聞くこと:** (4) プレビューに反映されないのは、値をドラッグしている間か、離した・打ち込んだ後もか（書き込みは変更から約 20 ms。ドラッグ中は設計どおり書かない）。
  (10) の Math の並びが直ったか、見てもらう。

## 再開の手順（2026-11-28 深夜、計画 P1.5e 完了（`24c11c2`）。利用者への報告と試用の依頼で止める）

- **P1.5e は全部済み:** Task 1〜6 と最終レビュー（文脈なし、opus）。レビューの Critical 2・Important 5 と M1 を直した（`24c11c2`、各々に失敗を確かめた試験
  `tests/dxg5/test_final.cpp`）。**試験: `dxg5_test` ALL PASS（681 件）、AE `run_dx2_graph.ps1` 8・16・32 bpc DX2 GRAPH PASS、`run_dx2_p1_fixture.ps1` PASS。**
- **実装時の判断:** 設計書 v5 の 19 節の最後の項に一覧（台帳は消した）。
- **後回しにした小さな指摘（利用者に報告済み）:** `expr(index: 3)` の名前のぶつかりが注記だけ、`SameGraph` が向きで答えが変わりうる、
  グループを直したとき使う側のグループの def を作り直さない、Ctrl+G の構造の入力が Float のまま、`OutputsOf(expr)` の毎回の読み直し、グループの出口ごとの再評価。
- **未解決のトラブル:** AE の警告「After Effects エラー: 内部確認に失敗しました。{no current context}」が setup の直後（Idle Hook の最初の書き込みより前）に
  出て試験が止まる。今夜 7 回中 3 回（8・32 bpc）、Task 9 でも 1 回。AE の中は追っていない。続けて 3 回 PASS したので、今回の変更の結果は確かめられた。
- **利用者の回答待ち:** 試用（下）。G->G（v5 16 節の 3、保留）の答え。積み重ね表示（前の計画の Task 12・13）は G->G の答えの後。
- **利用者に試してもらうこと（自動の試験で見ていない）:** 演算のノードの演算子の欄（クリックで一覧）、ノードの右クリック（Open・Copy for Myself・Ungroup・
  Make Group・Expand・Collapse）、Shift+クリックと Ctrl+G、Expression の式の欄（複数行、Ctrl+Enter）、Tab で開いた中の編集（自分のグループ・Expression）と
  ‹ Back、Macros の欄の unused の折り畳み、入力欄の右クリックの Connect Expression。

## 再開の手順（2026-11-28 夜遅く、計画 P1.5e の Task 1〜6 済み。Task 7（最終レビュー）中）

- **済み（続き）:** Task 5 `f6a102c`（自分用にコピー・Ctrl+G・Ungroup・Make Group・複数の出口の Collapse・unused のマクロ・台本の命令）、
  Task 6a `47487d2`（`OpenExpression`・`CloseExpression`・`SetGroupBody`）、Task 6 `d0902b1`（エディター: 演算子の欄・隠れた入力・赤い線・
  複数行の式の欄・入力欄は数だけ・Tab / ▸ で開く・右クリックのノードのメニュー・Shift+クリックと Ctrl+G・追加メニューに自分のグループ・unused の折り畳み）。
  **試験:** `dxg5_test` ALL PASS、AE `run_dx2_graph.ps1` 8・16・32 bpc DX2 GRAPH PASS（32 bpc は 1 回目に台本の送信中の AE の警告
  「…の確認に失敗しました。{no current context}」で止まり、2 回目 PASS。Task 9 からある止まり方で、文言が取れた）、`run_dx2_p1_fixture.ps1` PASS。
- **次の 1 手:** Task 7: 文脈なしのレビュー役にブランチ全体を見せ、Critical / Important を直して利用者に報告（台帳の Ruling を一覧で）。
- **利用者に試してもらうこと（自動の試験で見ていない）:** 演算子の欄のメニュー、ノードの右クリックのメニュー（Copy for Myself など）、Ctrl+G、
  式の欄（複数行・Ctrl+Enter）、開いた中での編集（自分のグループ・Expression）、unused の折り畳み。

## 再開の手順（2026-11-28 夜、計画 P1.5e の Task 1〜4 済み。利用者の指示「一旦中断してください」で止めた）

- **計画:** `docs/superpowers/plans/2026-11-28-dx5-review-e.md`（7 タスク）。台帳 `.superpowers/sdd/2026-11-28-dx5-review-e/progress.md`
  （git には入らない。`Task N: complete` の行と `Ruling:` の行がある）。実行は superpowers:executing-plans（自分で実装）。利用者は計画を承認済み（「進めてください」）。
- **済み（commit）:** Task 1 `d4ef3cf`（演算のノードは 5 つの種類＋演算子 `op=`、旧 `math_add` の読み替え、効かない線は残す）、
  Task 2 `3a60a0d`（Expression の式: 複数の出口・`_` の行・入口は知らない名前・`SetFormula`・`random()` の塩・`attr("ix")`、入力欄は定数だけ `SetConstText`・`ConnectExpression`）、
  Task 3 `c92b741`（コード: `expr(k: 3) { … }`・`w.x`・`math_sin(x, _b: y)`・式の中の名前付きの属性は `attr()`・`SameGraph` は値のノードの key と入口の名前を比べない）、
  Task 4 `fc93464`（利用者のグループ: コードの `group`・`return (a: x, b: y)`・`r.a`・保存 `group <name> 0 … end`・評価・構造の入力の自動判定）。
  **試験: `build\dxg5\dxg5_test.exe` ALL PASS（566 件）。v2（`tools/build.ps1 -Target v2`）もビルドが通る。** AE の試験はまだ回していない（Task 6 で回す）。
- **次の 1 手: Task 5**（編集の操作: `CopyGroupForSelf`・`GroupSelection`（Ctrl+G）・`UngroupNode`・`ExpressionToGroup`・`CollapseToExpression` の複数の出口・
  `UnusedNamedMacros`・`ExposeRefusal`・台本の命令 `op`/`formula`/`connectexpr`/`copygroup`/`group`/`ungroup`）。その後 Task 6（エディター・AE 試験）、Task 7（全体の試験・最終レビュー）。
- **気を付けること:** グループの本体は「それより前のグループ」を持つ（台帳の Task 4 の Ruling）。Task 5 でグループを足し替えるときは本体を作り直す。
  前の計画の Task 12（積み重ね表示）は G->G（v5 16 節の 3）の答えが出るまで止める。

## 再開の手順（2026-11-28 18 時台、設計の見直しを利用者が承認。計画 P1.5e を書いた。利用者の確認待ち）

- **利用者が v5 の見直しを承認（「これで進めてください」）。** 計画: `docs/superpowers/plans/2026-11-28-dx5-review-e.md`（7 タスク: 演算子を選ぶ演算のノード →
  Expression の式 → コードの読み書き → 利用者のグループ → 編集の操作 → エディター → 全体の試験）。実行は前と同じく自分で（superpowers:executing-plans）の予定。
- **前の計画の Task 12（積み重ね表示）・13 は、G->G（v5 16 節の 3、保留）の答えが出るまで止める。**
- **次の 1 手:** 利用者に計画を見てもらい、よければ Task 1 から。台帳は `.superpowers/sdd/2026-11-28-dx5-review-e/progress.md` に作る。

## 再開の手順（2026-11-28 17 時台、Task 12 の前に利用者が設計を見直し中。brainstorming、利用者の回答待ち）

- **Task 12 は保留**（コードの変更なし。最後の実装の commit は `a371944`）。利用者の指示: Task 12 の前に設計を見直す。
- **利用者が出した見直し（助言者の案）:** `G = { elm: G, conv: mat4 }[]`（木）。配置は `(...) -> (G -> G)` の関数（`linear(cnt, angle)`、
  `grid(cnt: int3, ofs: float3)`）。ノードの結果は G->G、合成は `grid([2,2,1],[10,10,0]) |> grid([3,2,1],[50,50,0])`。
- **利用者の質問 2 つ:** (1) Math ノードをコードで `math_xxx()` と書き、コードの式を入力の Expression にするのはなぜか（Math ノードに展開・単純な数式にしない理由）。
  (2) Math ノードを演算子ごとに分けず、Blender のように 1 つの Math ノードに演算子の選択を持たせたい。
- **返答した内容:** (1) 理由は往復で「Math ノードか Expression ノードか」の区別を戻すためだけ（設計書 v5 の 8.1・8.2 節、台帳の Task 6 の Ruling）。
  意味は変わらないので弱い理由。選択肢 (a) 今のまま (b) すべて数式で書き、Math の塊は往復で Expression にまとまる (c) Expression をやめ式は Math の木に。
  エディターには「式をノードに展開」（`dx2_editor_win.cpp:966`）だけあり「式にまとめる」は無い。(2) 賛成。触る所: `math_*` の定義・評価の分岐・展開・lower の `math_not`/`math_and`・追加メニュー・保存の名前の読み替え・演算子で入力の数と出力の型が変わる。
  G->G: 積み重ね表示＝ `|>` の並びで相性が良い。安い道: G を受ける配置を `instance(配置の点, 入力)` と読めば v5 の Instance と `parent.` の上の書き方で済む。
  `{elm: G, conv}[]` の穴: 葉（レイヤーの 1 コピー）が無い、属性（id・opacity・source・ix など）の置き場、`|>` の向き、木の上の index・position、Morph・Merge・Sort・at。
- **利用者の決定（続き）:** Math ノード 1 つに演算子（Blender と同じ。線は残して入力を隠す）。G->G は保留（利用者が助言者に聞いている）。
  入力欄の式はやめる方向。Expression ノードを「式を書くと出口・入口のソケットが増減し、開くと中は Math などの組み合わせ」にする
  （Math Controller 風、グループのように開ける）。中には式で書けるノードだけ置く（利用者が確認）。
  返答: 定数だけの式は入力欄で値にしてよい、積み重ね表示は欄で式を見せてつながる Expression を直す、Expression＝使い回さないその場の中身・
  グループ＝名前付きで使い回す定義、と分ける。Grid などは核ではすでにグループ（`src/dxg5/dx5_groups.cpp` に定義のコード、C++ の差し替え、
  一致の試験）。開く・自分用にコピー・Ctrl+G・コードでの `group` の定義（`dx5_lower.cpp:688` で拒む）は計画の範囲外（計画 302 行）。
  グループ＝コードでは関数の定義、に賛成（v5 の 8.1 節と同じ）。出口が複数のとき・外の変数を読めるか・マクロを読めるかの案を出した。
- **（17 時台後半）利用者が案を承認（「よいです」）。G->G は保留で残す（v5 の 16 節の 3 に記録）。設計書 v5 を直した（`084936d`）:**
  6.2 節（演算子を選ぶ Math・Vector Math・Compare・Boolean Math・Rotate）、7 節（`op=`、`math_*` の読み替え）、8.1〜8.3 節（入力欄は定数だけ、
  Expression ノード、`expr(k: 3) { x = …; y = … }`）、12.1 節（グループ＝関数）、13・14・15・17 節（P1.5e）、19 節。
  **レビュー済み（質問 11 件）。10 件は筆者の判断で v5 に反映（19 節の最後の項）。利用者に聞いているのは v5 の 16 節の 4（式の中の `ix` を
  `attr("ix")` で書くか、つないだ先にあれば裸で読むか）。** 答えを v5 に書き、承認をもらったら計画（P1.5e）を書く。
  **利用者は A（`attr("ix")`）を選んだ → v5 の 16 節の 4 に記録。** 続けて利用者の質問「使わなくなったマクロの扱い」に答えた（設計書には
  隠す規則（v4 の 1.1・8.1 節）と Expose の空きの定義（v4 の 8.1 節）しかない。名前・範囲・値・キーの後始末は決まっていない）。案（解放の操作など）を出した → **利用者の決定: 解放は作らない。使っていないマクロは隠し、名前付きは unused に畳む（v5 の 4.4 節）。**
  **利用者の回答待ち: 直した v5 全体の承認**（19 節の夕方の 3 項）。承認後に writing-plans で P1.5e の計画。
  （以下は前の状態）文脈なしのレビュー役（サブエージェント）に読ませている。 返った質問を「文書で答えが出る / 筆者の判断 / 利用者に聞く」に分け、直した v5 を利用者に渡して承認をもらう。
  承認後に計画（P1.5e）を書く。積み重ね表示（Task 12）は G->G の答えに関わるので、作るのは答えの後（と v5 の 17 節に書いた。利用者に確かめる）。
- **（古い）次の 1 手:** 利用者の答え（G->G で何を良くしたいか、上の案への意見）を聞いて、案を並べて選んでもらい、設計書 v5 を直す（承認まで実装しない）。
  そのあと計画を直して Task 12 以降へ。

## 再開の手順（2026-11-28 17 時ごろ、Task 11 済み・Task 12 の着手前。利用者の指示でセッションをクリア）

- **計画と台帳は下の節と同じ**（`docs/superpowers/plans/2026-11-28-dx5-graph-code.md`、台帳 `.superpowers/sdd/2026-11-28-dx5-graph-code/progress.md`）。
  台帳に `Task 11: complete (commits ec19b73..a371944)` がある。
- **Task 11 済み（`a371944`、`src/v2/dx2_editor_win.cpp`）:** ホイールでカーソル中心に縮尺（0.3〜3、縮尺ごとにフォントを作り直す、Home で全体）、
  線のスナップ（24 px 以内の型の合う入力）、つながった入力からドラッグで線を持ち上げる、Macros の欄の行を値へドラッグで LinkMacro、
  欄は「I Int Macros」「M Macros」の見出しで分ける、追加メニューの先頭に Search...（打って Enter で絞った一覧、1 件ならそのまま追加）、
  つながった入力は値の欄を出さない（式とマクロの名前だけ薄く）、Each の枠、グループの題名の ▸ で Code 表示のその行へ。
  **AE 試験 `run_dx2_graph.ps1` は 8・16・32 bpc とも DX2 GRAPH PASS**（スナップと `editor: zoom 1.331` を含む）。`dxg5_test` 399 件 PASS。
  持ち上げ・マクロのドラッグ・Search・Home・▸ は自動の試験で見ていない（台帳に Ruling。利用者の試用で確かめてもらう）。
- **次の 1 手（Task 12、積み重ね表示）:** `src/dxg5/edit.h` の `StackRow`・`StackRows` は宣言だけ（`branches` は無く `depth` で枝を表す。
  計画の `branches` は使わない、と台帳に Ruling を書く）。`dx5_edit.cpp` に実装: Output から geometry の入力をさかのぼり上流を先に並べる。
  geometry の入力が 2 つ以上なら各枝を depth+1 で先に。ある出力を 2 か所以上の入力が使うノードから上は `collapsedGraph` の 1 行。
  `each` は `geometry` の入力だけたどる（`body` はゾーンの中）。試験は `tests/dxg5/test_edit.cpp` の末尾に `TEST(stack_rows_default)`
  （既定のプリセットは grid の key が `g`、Output が `out`。計画の `grid1` ではない → 型で確かめる）と `TEST(stack_rows_shared)`
  （`g = grid(...)`、`a = transform(g)`、`b = delete(g)`、`output(a)` → 先頭が `g` の collapsed）。合流のノード（merge 等）は今は無い。
  そのあと `src/v2/dx2_editor_stack.cpp`（または同じ窓のファイル。Task 10 の判断に合わせてよい）と AE 試験（Stack タブで grid の Spacing に 150）。
  続けて Task 13（全体の試験・最終レビュー・利用者への報告）。
- **気を付けること:** 下の節と同じ（v2 は `src/graph` と `src/dxg5` の両方をコンパイル、dxg5 の .cpp は `dx5_` で始める、作業ツリーの改行コードだけの変更は触らない）。

## 再開の手順（2026-11-28 16 時台、設計書 v5 の実装中。計画の Task 9 まで済み。利用者「aex で確認できるまで進めて」）

- **計画:** `docs/superpowers/plans/2026-11-28-dx5-graph-code.md`（13 タスク）。台帳（進み具合と判断の記録）は `.superpowers/sdd/2026-11-28-dx5-graph-code/progress.md`
  （git には入らない）。再開したら台帳の `Task N: complete` を見て、次のタスクから続ける（superpowers:executing-plans、自分で実装）。
- **済み（`9f71473` まで）:** 新しい核 `src/dxg5`（型・値の線・Field・コードの読み書き・組み込みのグループと C++ の差し替え・編集の操作、
  `tests/dxg5` 399 件 PASS）。エフェクトを dx5 に切り替え、Int Macro 1〜32（Disk ID 69〜102）を追加、既定の格子の Count は Int Macro 1。
  ノードの窓は dx5 用に作り直した（値の線・式の入力・Int Macro への Expose）。
  AE 試験: `run_dx2_graph.ps1` が 8・16・32 bpc で PASS（32 bpc は 4 回中 1 回、台本の送信中に AE の警告で止まった。文言は未取得）。
  **P1 で保存したプロジェクト（`tests/ae/fixtures/dx2_p1.aep`）が開き、同じ要素を描く**（`run_dx2_p1_fixture.ps1` → P1 FIXTURE PASS）。
- **Task 10 済み（Code 表示）:** タブ Nodes / Stack / Code。Code で書いて Apply（Ctrl+Enter）すると AE の絵が変わる。誤りは行と列つきで拒む。
  AE 試験は 8・16・32 bpc で PASS（Code のタブ・誤りのコード・正しいコード）。**ここで利用者が試せる aex になった。**
- **中断（利用者の指示でセッションをクリア、16 時台後半）: Task 11 の途中。** AE 試験だけ先に書き、**期待どおり落ちることを確かめた（RED）**:
  `run_dx2_graph.ps1` に「ソケットから (14, 10) ずれた所で離してもつながる（スナップ）」と「Nodes タブに戻ってホイール 3 回 → 記録に
  `editor: zoom 1.331`」を足し、`check_dx2_graph.py` に 2 項目（`write (wire)` と `zoom 1.331`）を足した。`ae_bg.ps1` の `AeBg` に
  `ClientToScreen`・`PT` を足した。今の aex では 8 bpc で この 2 項目だけ WRONG（ほかは ok）。
- **次の 1 手（Task 11 の実装、`src/v2/dx2_editor_win.cpp`）:** 縮尺 `zoom`（0.3〜3、ホイールでカーソルの位置を中心に ×1.1、
  変えたら `Log("editor: zoom %.3f")`、レイアウトの全関数とフォントに掛ける、Home で全体が入る縮尺）、スナップ（線のドラッグ中、
  24 px 以内でいちばん近い型の合う入力に吸い付き、離すとつなぐ）、入力からドラッグで既存の線を持ち上げる、Macros の欄の行を値の欄へドラッグで
  `LinkMacro`、追加メニューの先頭に「Search...」（入力欄で名前の一部を打って Enter）。そのあと 8・16・32 bpc で PASS を確かめて commit。
  続けて Task 12（積み重ね表示、`StackRows` は `src/dxg5/edit.h` に宣言だけある）→ Task 13（全体の試験・最終レビュー・利用者への報告）。
- **台帳の判断（利用者に最後に一覧で伝える）:** `//` は注記だけ・床の割り算は `floordiv()`、Math ノードは `math_add()` の形、
  グループ Source は `source_layers`、each の中の新しい名前はローカル（名前付きの属性は `store("名前", 値)`）、ほか。
- **気を付けること:** v2 のプロジェクトは `src/graph`（版 1 の読み込み）と `src/dxg5` の両方をコンパイルする。dxg5 の .cpp は `dx5_` で始める（obj の名前がぶつかる）。

## 再開の手順（2026-11-28 15 時台、設計書 v5 を書き、文脈なしのレビュー役に読ませている）

- **利用者の設計の見直し（brainstorming で話し合って決定）:** 1 つのグラフを「ノード・積み重ね・コード」の 3 通りで見せる。コードはグラフをそのまま書いたもの
  （ノード＝関数・線＝引数、助言者の指摘）。Position などは Blender の Field（つないだ先のノードの並びで評価）。ループは Each ゾーン（要素ごと・並列）と
  Repeat ゾーン（状態を持ち越す・順番）で、中身の見えない Script ノードは作らない。Grid・Circle などは組み込みのグループ（開ける・コピーして変えられる）で、
  速さは C++ の実装への差し替え（定義と一致を試験）。すべて `docs/design/複製プラグイン設計案_v5.md`（`32b4e0c`）に書いた。
- **レビュー 2 回済み（`e59df7f`）:** 1 回目 11 件・2 回目 14 件を反映（v5 の 19 節に一覧）。Field の書き分け（5.1 節）は利用者が決定。
  **利用者の回答待ち: v5 の承認。** 積み重ね表示の向き（上が上流）と置き場所（エディターの中だけ）は仮の決定で、承認のときに確かめてもらう。
  承認されたら superpowers:writing-plans で P1.5a〜d の計画を書く。
- **（済み）** v5 を文脈なしのレビュー役（サブエージェント、v5 と v4 だけを読む）に読ませた。返ってきた質問を「文書で答えが出る / 自分で決める（筆者の判断と書く）/
  利用者に聞く」に分け、利用者に聞くものは 1 回にまとめて聞く。直した v5 を利用者に渡して承認をもらう。承認後に writing-plans で計画（P1.5a〜d）。
- **利用者の回答待ちになる予定:** v5 の 16 節（積み重ね表示の向き、Overview にも出すか）とレビューの「利用者に聞く」分。
- **実装はまだしない**（承認前。brainstorming の決まり）。

## 再開の手順（2026-11-28 14 時台後半、利用者「設計を見直します。一旦作業を中断してください」。利用者の話を待つ）

- **済み:** 試用の指摘 3 つ（下の節）。1.3 節の 4 点に利用者が回答し、設計書に記録した: Int Macro は 32 個（Disk ID 69〜、グループ「Int Macros」）、
  浮動小数のマクロは 32 個のままで足りる、Position・Index は Modulator と同じ扱い（構造の入力にはつながない）で確定。**数式は保留**（利用者が設計を見直す）。
- **中断したところ:** 実装には入っていない（コードの変更なし）。Position・Index にはノードからパラメータへ値をつなぐ線（Field）が要り、
  今のグラフは線がジオメトリだけ（`Wire` は `from`・`to`・`port`）なので、データの形・評価・エディターに手が入る。設計書の 2.3 節（Modulator）を読み始めたところで止めた。
- **次の 1 手:** 利用者の見直しの結果を聞き、設計書に反映してから着手する。勝手に実装を始めない。

- **(3) 重い → 済み（`e8ebe59`）:** `dx2_perf.jsx`（キャッシュを毎回捨てる）で 1920×1080 のコピー 25 個が 2,315 ms → **71 ms**（8 bpc）・58 ms（32 bpc）、
  200×200 は 28.5 → **3.8 ms**。小さいレイヤーの残りの大半は `Parallel` が毎回 `std::thread` を 7 本作る費用（AE は DLL が多く、1 回約 13 ms）で、
  PPL の `parallel_for` に替えて消えた。**絵の確認:** `a612df7`（高速化の前）の `dx2_render.cpp` でビルドした `run_dx2_graph.ps1 -Bpc 32` の PNG 14 枚と
  今の PNG が画素の差 0（回転・不透明度 50% の B を含む）。8・16・32 bpc とも DX2 GRAPH PASS。GPU の案は不要になった。
- **利用者の回答待ち:** 1.3 節の「設計への反映（案）」の 4 点（Int Macro の数と置き場所、Macro 32 個で足りるか、数式を残すか値だけか、
  Position・Index を Modulator と同じ扱いにするか）。回答をもらったら 1.3 節を固め、エディターの要望（Blender 風の操作・スナップ・ホイールの縮尺・
  Macros の欄の見切れ・Macros からドラッグ）に着手する。
- **気を付けること:** 古いソースを一時的に戻して比べるとき、戻したファイルの更新時刻が古いと MSBuild が再コンパイルしない（今回 aex が古い版のまま残りかけた。
  大きさで気付いた）。戻したあとは時刻を更新してビルドする。作業ツリーの `src/core`・`src/gpu` などの変更（会話の前から）は触っていない。

## 再開の手順（2026-11-28 15 時台、利用者の試用を受けて直している途中。利用者の指示で中断、セッションをクリア）

- **試用の指摘 3 つのうち 2 つは直して確かめた（`a612df7`）:** (1) ソケットがつながらない → `WM_LBUTTONUP` で `ReleaseCapture` を先に呼んでいたため
  （`WM_CAPTURECHANGED` がドラッグを終わらせていた。値のクリックで入力欄が開かないのも同じ原因）。(2) Circle のプリセットでマクロしか変わらない →
  UI のイベントの中で Arbitrary Data をその場で書き換えても AE が受け取らなかった。プリセットは Idle Hook の書き込み（`QueuePreset`）に替えた。
  `run_dx2_graph.ps1` は窓にマウスの操作を送って確かめる（コンプ D・E）→ PASS。
- **(3) 重い（途中）:** 測った結果（`tests/ae/dx2_perf.jsx`、`run_jsx.ps1 -Jsx tests/ae/dx2_perf.jsx -Name dx2_perf`）: 1920×1080 のレイヤーの
  コピー 25 個で描画 2,315 ms（1 画素約 280 ns。並列は 8 スレッドで効いている）。1 回目の直し（元を一度だけ premultiplied の float に変換、
  縁付きで範囲の判定なし、行ごとに区間を計算）で 958 ms。**今の作業ツリー（commit 済み、未計測・未試験）:** SSE の Bilinear・Over、平行移動の近道
  （画素が揃えば 1 タップ）、変換のバッファを thread_local で使い回し。**次の 1 手:** `dx2_perf.jsx` で測り、`run_dx2_graph.ps1`（8・16・32 bpc）で
  絵が変わっていないことを確かめる。マシンは i7-6700（4 コア 8 スレッド）で、ほかのプロセス（python・codex-bridge）が CPU を常に 30% ほど使う。
  それでも遅ければ、今の DupliX の GPU の Fast（`src/gpu`）を使う案を利用者に出す。
- **利用者の要望（設計書 v4 の 1.3 節に記録。この後に着手）:** エディターは Blender のノードエディターを手本に（操作感・ノードの種類、Position・Index の
  ノード、トランスフォームするノード）、ソケットのスナップ、値の入力に数式、ホイールで縮尺・ミドルドラッグでスクロール、Macros の欄の説明が見切れている、
  Macros の欄からパラメータへドラッグでつなぐ、整数のマクロ（Int Macro、数は浮動小数と同じで十分に）。1.3 節の「設計への反映（案）」は利用者に確かめてから固める
  （Int Macro は Disk ID 69〜に足す、要素ごとの値のノードは Modulator と同じ扱い、数式は値で保存か式を残すか）。
- **利用者の質問への答え（報告済みにする）:** Effect Controls のマクロの名前が変わらないのは意図どおり（P0: 付け替えると開き直しでエクスプレッションが壊れる）。
- **気を付けること:** 動いている AE に台本を送る試験は `Send-AeScript`（`spike/tools/ae_bg.ps1`）を使う。GPU の取り合いの再現は `tools/gpu_load.ps1`。

## 再開の手順（2026-11-28 11 時台後半、P1 の (a)(b)(c) 済み。利用者に試用を頼んで回答待ち）

- **済み（`8872523` まで）:** 核（`src/graph`、176 件 PASS）、DupliX 2 の作り直し（`src/v2`、パラメータ 69 個、設計書 8.1 節）、エディターの窓（`dx2_editor_win.cpp`）。
  `pwsh tests/ae/run_dx2_graph.ps1` → **DX2 GRAPH PASS**（キーを打ったマクロで格子と円が動く、エディターの台本・取り消しでの読み直し・プロジェクトを閉じたときに閉じる）。
- **利用者の回答待ち（試用）:** AE で「DupliX 2 (Preview)」（カテゴリ DupliX）を平面に掛ける → Overview の Open Editor → 右クリックでノードを足す・
  出力から入力へドラッグでつなぐ・値をクリックで入力 / 左右ドラッグ・値の右クリックで Expose to Macro → Effect Controls の Macro にキー。
  Overview の Grid / Circle でプリセット。気になる点（操作の感触、足りない操作）を聞く。
  **特に確かめてもらう（自動の試験で見られない）:** エディターで Delete を押したとき AE の選択中のレイヤーまで消えないか、
  値の入力欄で数字・`-`・`.`・スペースを打ったとき AE のショートカットが反応しないか、Ctrl+Z が 1 回分だけ戻るか（2 回戻らないか）。
  **エディターを開いたままエフェクトを消さない**ようお願いした（消したエフェクトへの書き込みは未確認）。
- **残りの課題:** (1) エフェクトを消しても `SEQUENCE_SETDOWN` が来ないので、エディターはプロジェクトを閉じるまで開いたまま（消したエフェクトに書くとどうなるかは未確認）。
  (2) （解決、12:50）「動いている AE に送った台本がたまたま届かない」の原因は AE の警告「2 つめのスクリプトは実行されませんでした」
  （題名のない `#32770`。前の台本の最後の行から数秒たっても AE が実行中と数えている。GPU が混むと起きやすい）。共通の `Send-AeScript`
  （`spike/tools/ae_bg.ps1`）が警告を閉じて送り直し、ほかの警告は待たずに中身を出して止める。**動いている AE に台本を送る試験は必ずこれを使う。**
  再現には `tools/gpu_load.ps1`（GPU を埋める）。窓は前面に出して開く（`SW_SHOWNORMAL`）。
  (3) P1 は Fast だけで描く（Render Mode の 3D Layers も同じ絵。P6）。描く範囲はコピーの外接箱（Canvas は後）。
- **次の 1 手:** 試用の結果を直す → P2（Morph・Merge・Instance、Polar・Sphere・Path・Fibonacci・Scatter・Point）。
- **気を付けること:** 試験の書き込み（`DUPLIX2_TEST_SCRIPT`）は 1.5 秒待つ（台本の実行中に書くと `AEGP_SetStreamValue` が戻らない）。SmartRender はレイヤーの画素を出力より先に取り出す。

## 再開の手順（2026-11-28 11 時台、P1 を進めている。利用者の指示: P1 に着手してよい）

- **進め方（助言役と合意）:** 区切りは (a) 核と単体テスト → (b) パラメータと描画（既定のプリセット）と jsx の試験 → (c) エディター。各区切りで commit・push・このメモ。
- **(a) 済み（`3bedd07`）:** `src/graph`（graph・eval・edit。AE も Win32 も含まない）、`tests/graph/graph_test`（`pwsh tools/build.ps1 -Target graph`、176 件 PASS）。
  `graph_test.exe eval <graph> <t> m1=..` は評価の結果を、効果の書き出し（`DUPLIX2_DUMP`、これから作る）と同じ形（`FormatElements`）で出す。
  `graph_test.exe script <script> <out>` はエディターの操作の台本（`RunScript`）を流す。パラメータの表・マッチ名・グラフデータの形は設計書 v4 の 8.1 節で固定した。
- **(b) 済み（`672490e` と次の commit）:** `src/v2` を v4 のパラメータ 69 個に作り直した（`dx2_flags.h` を PiPL と C++ で共有、`static_assert`）。描画は逆アフィン
  （AE の画素は straight alpha、中で premultiplied）。Overview に配置の図・使っているマクロ・Open Editor（まだ何もしない）・プリセット Grid / Circle。
  試験 `pwsh tests/ae/run_dx2_graph.ps1` → **DX2 GRAPH PASS**（PreRender ごとの書き出しが `graph_test eval` と一致、画素は AE 自身の 50% の平面と一致、取り消しで既定の格子に戻る）。
  ハリボテで保存した aep は v4 で「ファイルのデータが見つかりません」と出て開かない（落ちない。設計書 8.1 節）。
- **途中で分かったこと（トラブル表に書く）:** (1) 書き込みを queue した jsx がまだ動いている間に Idle Hook が `AEGP_SetStreamValue` を呼ぶと戻らない
  （試験の書き込みは 1.5 秒待つ。エディターの書き込みは利用者の操作なので台本と重ならない）。(2) SmartRender で `checkout_output` を先にすると、入力の
  `checkout_layer_pixels` が `PF_Err_BAD_CALLBACK_PARAM`（516）になる（レイヤーを先に取り出す）。
- **次の 1 手 (c):** エディターの窓（`dx2_editor.cpp` の `OpenEditor`）。Win32 で、ノードの箱・つなぎ・値の編集・マクロに出す・ノードの追加と削除。
  操作は `graph/edit.h` の関数だけを呼ぶ。書き込みは今の queue（`g_queue`）。版の番号の同期と `SEQUENCE_SETDOWN` で閉じる。
- **(c) の予定:** P0 の Idle Hook の書き込み・`PF_RefreshAllWindows`・版の番号の同期・`SEQUENCE_SETDOWN` で閉じる、を移す。試験はエディターの操作の層を台本で動かし（`DUPLIX2_TEST_SCRIPT`）、結果のグラフデータを確かめる。
- **古い試験:** `v2_sources.jsx`・`check_v2_sources.py`・`check_ui.py` は 409 個のハリボテ用。作り直しに合わせて消すか書き直す。
- **気を付けること:** 作業ツリーの改行コードだけの 11 ファイルは触らない（`git add` はファイルを指定する）。

## 再開の手順（2026-11-28 10 時台、P0 完了・未決 5 も回答済み。次は P1）

- **未決 5 の回答（9.1 節に記録）:** Optimize オンで色・別の Source に対応できないコピーは、そのコピーだけ組から外して Optimize オフと同じ描き方にする。
- **次の 1 手:** P1（グラフの核）に着手。以下は 1 つ前の状態。

- **済み:** 利用者の目視で (1) スライダーの範囲の付け替えが Effect Controls に出る、(2) .ffx にグラフデータが入る、を確認 → 設計書 v4 の 8 節に記録し P0 を閉じた。
  目視中に出た「グローバルアウトフラッグが不一致（コード 4008000、PiPL 2008000）」は `spike/P0Graph/P0GraphPiPL.r` の値の誤り
  （SEND_UPDATE_PARAMS_UI は 1<<26）。`0x04008000` に直してビルド、`run_p0_graph.ps1 -Path idle -Refresh` PASS。
- **利用者の回答待ち:** 9 節の未決 5（3D Layers でコピーごとの色・別のレイヤーの Source ができないとき、主に Optimize オンの組の描画で対応できない場合の扱い）。
- **次の 1 手:** 回答を 9.1 節に書いて P1（グラフの核）へ。
- **残っている確認:** 利用者のプロジェクトの比較 Test・B・C（10 時もまだ GPU が使用率 100%・11.8 GB で使われている）。
- **気を付けること:** 作業ツリーの 11 ファイルの変更は改行コードだけ（`git diff --ignore-cr-at-eol` で差なし）。触っていない。

## 再開の手順（2026-11-28 7 時台、設計書 v4 の P0 がほぼ済み。利用者に報告し、目視 2 点を頼んで回答待ち）

- **済み（設計書 v4 の 8 節の「P0 の結果」に一覧）:** 書き換えの経路＝Idle Hook、取り消しの同期、書いた後の描き直し（`PF_RefreshAllWindows`）、
  エディターの寿命（`SEQUENCE_SETDOWN`）、マクロの名前は付け替えない、評価の速さ（1 万要素 約 5 ms）、Layer Alpha（ハッシュで作り直し）、
  3D Layers のフレームごとの数の変化・コピーごとの色（`DUPLIX_DBG_TINT`）・別のレイヤーを写すコピー（`DUPLIX_DBG_OTHER`、大きさ違い・時間のずれも一致）。
  試作はどれも環境変数で入り、無いときは何もしない。**既定の動作の確認（7 時、環境変数なし）:** vector_probe 全部・CANVAS・smoke・FAST・
  MERGE（MFR オフ・オン）すべて PASS。色と別のレイヤーは Optimize オフでだけ確認（オンは組の描画の対応が要る）。
  実験エフェクトは後片付け（`SEQUENCE_SETDOWN`、描画スレッドにも来る）で印を立てて窓に閉じる指示を送る形にした（開き直しの試験も PASS）。
- **利用者の回答待ち（目視）:** (1) スライダーの範囲の付け替えが Effect Controls に出るか、(2) .ffx（アニメーションプリセット）にグラフデータが入るか。
  手順は報告に書いた（「DupliX P0 (Spike)」を掛け、Open Editor → Red / Green → Macro 1 のスライダーの右端が 300 / 500 になるか。
  緑にした状態でアニメーションプリセットを保存し、別の平面に適用して緑で塗られるか）。実験エフェクトの書き込みは既定で Idle Hook・描き直しあり。
- **次の 1 手:** 目視の結果を設計書 v4 に書いて P0 を閉じ、9 節の未決 5（3D Layers でできないときの扱い）を利用者に確かめてから P1（グラフの核）へ。
- **残っている確認:** 利用者のプロジェクトの比較のうち Test・B・C（GPU がほかのプロセスに使われていて止まる。11/28 7 時もまだ 61%・11.8 GB）。
  GPU が空いたら `run_user_merge.ps1 -Aep "tests/ae/user/<名前>.aep"` で回し直す。
- **試験の道具（P0）:** `run_p0_graph.ps1 [-Path idle|direct] [-Payload n] [-Refresh]`、`run_count_keys.ps1`、`p0_alpha.jsx`（`DUPLIX_P0_SAMPLE`）、
  `text_twins.jsx`（`DUPLIX_TT_ONLY`、`DUPLIX_DBG_COPIES` の行列から本物を並べる。fill・other の場合）、`tools/bench_p0.ps1`。

## 再開の手順（2026-11-28 5 時台、設計書 v4 の P0 を進めている。利用者の指示: 報告を挟まずに P0 を進める）

- **利用者の指示（11/28 1 時台）:** 利用者のプロジェクトの比較が終わり、直すものがあれば直したら、報告を挟まずに設計書 v4 の P0 に進む。
  中断・報告のときは引き継ぎメモを残す。
- **利用者のプロジェクトの比較（`run_user_merge.ps1 -Aep "tests/ae/user/<名前>.aep"`）:** Test A・D は差 0。Test・B・C は AE が描画の途中で止まり時間切れ。
  **変更前の commit（1e05427）のビルドでも同じように止まった**ので今日の変更ではない。その間 GPU がほかのプロセスに使用率 87〜100%・メモリ 11.6/12 GB 使われていた
  （開発記録 11/27 のトラブル表）。**GPU が空いているときに Test・B・C を回し直す**（`nvidia-smi` で確かめてから）。
- **P0 の 1 つ目（済み）: エディターの窓からのグラフデータの書き換え → Idle Hook の経路に決定**（設計書 v4 の 7 節に結果）。
  実験エフェクト `spike/P0Graph`（`pwsh tools/build.ps1 -Target p0` → `dev_plugins\DupliXP0.aex`、マッチ名 DupliXP0）、
  試験 `pwsh tests/ae/run_p0_graph.ps1 [-Path idle|direct] [-Payload <bytes>]`（AE を開いたまま、jsx を 2 回送る。`-r` の AE はスクリプトの終わりに閉じるので
  `scheduleTask` は使えない）。idle は PASS（100 KB でも 7 ms）、direct は `AEGP_SetStreamValue` から戻らない。
- **P0 の 2・3 つ目（済み、`adc8347`）:** AE 側の取り消しとの同期（Idle Hook で版の番号を比べる）は動く。マクロの名前は付け替えない
  （付け替えた名前で書いたエクスプレッションが開き直しで壊れる）。エディターは `SEQUENCE_SETDOWN` で閉じる（閉じたプロジェクトの参照で AE が落ちた）。
- **次の 1 手（P0 の残り、助言役と決めた順）:** (1) フレームごとの数の変化（Copies にキー、3D Layers、Optimize と MFR のオン・オフ、各フレームを
  単独で描いたものと比べる）→ (2) コピーごとの色（`HookTex` で絵を染める。正解は本物の複製に Fill / Tint を掛けたもの、双子の試験で）→
  (3) 別のレイヤーを写すコピー（`DrawWholeText` と同じ要求の差し替えで、隠した平面 B を写す。範囲・時間・目のスイッチ）→
  (4) グラフデータを書いた後に Overview を描き直させる方法（`UPDATE_PARAMS_UI` がすぐ来ない）→ (5) 評価の速さのベンチ（要素 1 万）→
  (6) Layer Alpha のキャッシュの作り直し → Canvas Auto の上限は既存の Fit Copies（3,200 万画素）を引用。
  **.ffx とスライダーの範囲の付け替えは画面でしか確かめられない**ので、最後の報告で利用者に目視を頼む。
- **Test・B・C の止まり:** 今日の変更による後退ではないが、GPU の取り合いのときの止まり（`DUPLIX_GPU_FAIL=1` でも `gpu ae span` を通っており、
  GPU なしの実行はできていない）で、環境だけの問題とも言い切れない。GPU が空いたら回し直す。
- **気を付けること:** 実験エフェクトは `dev_plugins` に入るので利用者の AE にも「DupliX P0 (Spike)」として出る。

## 再開の手順（2026-11-28 1 時台、3D Layers のテキストを複製できるようにした。試験の残りを実行中）

- **済み（この節の commit、詳細は開発記録 14 節の「訂正と解決」）:** レビュー役の指摘（描画ノードは番号 −1 なら全文字を描く。
  普通のテキストもテクスチャの入口を通る）から、コピーのテクスチャの要求の間だけ番号 −1・テキスト自身のキャッシュに差し替える
  `DrawWholeText` と、`GetSourceFloatRect` の答え（文字の範囲）を入れた。テキストのフックは**既定でオン**（`DUPLIX_TEXT_HOOKS=0` で切れる）。
- **確かめた試験:** vector_probe 全部 PASS（text の nm1 は元のテキストと RGB 差 0、text_perchar は変わらず）、CANVAS PASS、smoke PASS、
  FAST ALL PASS、MERGE ALL PASS。`text_twins.jsx`（本物のテキストを並べた比較）: 回転は差 0、ほかは数画素だけ差 1
  （シェイプでも同じ種類の差。本物を jsx の値で作ると行列の最後の桁が変わるため。開発記録に記載）。
- **実行中だったもの:** `run_user_merge.ps1 -Aep "tests/ae/user/<名前>.aep"` を Test・A・B・C・D の順に（`-Aep` が必須）。
  結果を見ていなければ、もう一度回して各 8 フレーム差 0 を確かめる。
- **次の 1 手:** 上の結果を確かめて利用者に報告。その後は設計書 v4 の P0。
- **気を付けること:** 診断用の環境変数: `DUPLIX_DBG_TEX`（テクスチャの要求を全部記録）、`DUPLIX_DBG_COPIES=<file>`（コピーの M4 を書き出す）、
  `DUPLIX_DBG_VEH`、`DUPLIX_DBG_RENDER_AS_LAYER`。

## 再開の手順（2026-11-28 0 時台、3D Layers のテキストの解析の結果を報告。利用者の判断待ち）

- **済み（この節の commit、詳細は開発記録 14 節の「テキストの続き」）:** 描かれない原因は BEE の中の C++ の例外。
  `GetSourceFloatRect`・`GetSubLayerDepth` を AV へ回して 1 段先（テクスチャの要求）まで進んだが、その先のテキストの描画ノード
  （`BEE_TextRenderNode`）は 1 文字ずつしか描けず、AV のノードは素材のないテキストで失敗する（6667）。
  代案「文字 × コピー」は、AE 自身が文字単位 3D のオン・オフで違う絵を出す（`text_perchar_same.jsx`、7 通りすべて差あり）ので完全一致しない。
- **利用者の回答待ち:** (1) 普通の 3D テキストが通るレイヤー全体の経路をさらに探す（見込みは不明、時間がかかる）、
  (2) テキストの 3D Layers は諦めて Fast (2D) を案内する（今の挙動、文書は記載済み）、(3) 設計書 v4 の P0 に移る。
- **今の状態:** テキストのフックは既定でオフのまま（`DUPLIX_TEXT_HOOKS=1` で入る。今は 13 スロットを回す）。診断用に
  `DUPLIX_DBG_VEH=1`（最初に投げられた例外の呼び出し元を記録）と `DUPLIX_DBG_RENDER_AS_LAYER=1` を足した。
- **試験:** `text_perchar_same.jsx`（`run_jsx.ps1 -Jsx tests/ae/text_perchar_same.jsx -Name text_same` → `check_same.py build/test/text_same`）。
  既定の動作（テキストのフックはオフ）は変えていないので、平面・シェイプの試験は前回のまま。
- **気を付けること:** 例外の位置は `DUPLIX_DBG_VEH=1` のログ（`dbg: exception e06d7363 at ...`）の BEE.dll+オフセットを IDA（0x180000000 足す）で引く。

## 再開の手順（2026-11-27 22 時半、次は 3D Layers のテキストの解析。利用者がセッションをクリア）

- **利用者の指示（22 時半）:** 次の作業は「1. 3D Layers のテキストを続けて解析する」。設計書 v4 の P0 はその後。
- **今の状態:** 最後の commit まで push 済み。`dev_plugins\DupliX.aex` は最新のソース（テキストのフックは既定でオフ）。
- **分かっていること（開発記録 14 節）:**
  - テキストは `BEE_TextLayer` の vtable。AV と違う関数: IsLayerType(79)・SubLayerRenderCount(219、文字の数＝TextGrid の数)・
    SharedXformTimeCacheFactory(218)・GetSubLayerRenderBounds(220)・GetSubLayerXform2D(221〜223)・GetSubLayerOpacity(226)・
    GetSubLayerBlur(227)・GetBoundsFromCache(229)・HasSubLayerXform(234)・Rasterize2DGraph(235)・NewXformCache2D(237)・
    GetSubLayerLocalMatrix3D(168) ほか（番号は AE 25.6）。
  - `DUPLIX_TEXT_HOOKS=1` で、DupliX を掛けた（文字単位 3D でない）テキストに限り上の関数を AV の関数へ回す（`InstallTextRedirect`、
    `bee_abi.h` の `kTextRedirect`）。すると Count・Rects のフックが呼ばれ、配置まで DupliX の答えになる（ログ `hook: rects ... n=1`）が、
    その直後に何も起きずフレームが描かれない（`saveFrameToPng` が 10 秒で諦め、png ができない。AE は落ちない）。
  - 文字単位 3D の見分けはスイッチのビット 0x80000（`kSwitchPerChar3D`。普通 0x10487、文字単位 3D 0x90487）。
- **次の 1 手（解析の入口）:**
  1. `DUPLIX_TEXT_HOOKS=1` で `vector_probe` の text だけを回し（下のコマンド）、Rects の後に呼ばれる関数を突き止める。
     手がかり: シェイプ・平面では Rects の後に `HookArtOpacity`（IAT）とテクスチャ（`HookTex`／`BEE_RenderTextureForArtisan`）が呼ばれる。
     テキストでそれらが呼ばれているか、`DUPLIX_CMDLOG=1` や一時的なログで確かめる。
  2. IDA（idalib の MCP、データベース `spike/ida/BEE.dll.i64`、イメージベース 0x180000000）で `BEE_RenderTextureForArtisan` から
     テキストの文字の描画（`TXT_` の関数、`BEE_TextLayer::GetTextGrid`、`sub_1805AB430`＝キャッシュから文字の並び）への経路を読み、
     テキスト用キャッシュ（`SharedXformTimeCacheFactory` の text 版が作るもの）を前提にする箇所を探す。
     テキストの Rasterize2DGraph（0x1805AE150）は、形の行列の行列式が小さいとき `sub_1805B0250` で文字ごとのノードを作る。
  3. 直せないときの代案: テキストの 3D Layers はコピーを AE に描かせず、Fast (2D) を案内する（今の挙動のまま、文書は記載済み）。
- **試験のコマンド:** `$env:DUPLIX_TEXT_HOOKS='1'; $env:DUPLIX_VP_ONLY='text,text_perchar,shape'; pwsh tests/ae/run_jsx.ps1 -Jsx tests/ae/vector_probe.jsx -Name vector_text -TimeoutSec 400 -Expect "^done"`
  → `spike/.venv/Scripts/python.exe -I tests/ae/check_vector.py build/test/vector_text`（nm1_text が MISSING なら描かれていない）。
  直したら、全部の場合・`check_canvas.py`・`run_fast_mode.ps1`・smoke・`run_merge.ps1 -Mfrs OFF,ON`・`run_user_merge.ps1`（Test〜D）で確かめる。
  AE を動かす試験は 1 つずつ。長いもの（merge・user_merge）は 10 分を超えるので裏で回す。
- **気を付けること:** テキストのフックを既定でオンにするのは、描けることと、DupliX を掛けていないテキスト・文字単位 3D のテキストが
  変わらないこと（`text_perchar` の nm＝plain）を確かめてから。利用者の管理者 PowerShell では `cmd` が devkitPro のものを指すので、
  案内するコマンドは PowerShell の命令で書く。

## 再開の手順（2026-11-27 22 時台、シェイプ対応・Fast の原点・Canvas が済み。テキストの 3D Layers は未解決。利用者への報告待ち）

- **済み（この節の commit、詳細は開発記録 14 節）:** Fast (2D) でシェイプ・テキストの位置と範囲を直した。3D Layers でシェイプを複製できる
  （vtable ごとの元の関数）。Rendering に Canvas（Comp Size / Fit Copies、Disk ID 39）。使い方の文書に「Canvas」「シェイプレイヤーとテキストレイヤー」。
- **未解決:** 3D Layers のテキスト。サブレイヤーの関数を AV に回すと配置までは通るがフレームが描かれない（既定でオフ、`DUPLIX_TEXT_HOOKS=1`）。
  次に調べるなら、コピー 1 枚ずつの絵を作る経路（`BEE_RenderTextureForArtisan` → テキストの文字の描画）で何がテキスト用キャッシュを読むか。
- **試験:** vector_probe（`check_vector.py`・`check_canvas.py`）PASS、FAST ALL PASS、smoke、MERGE ALL PASS、`run_user_merge.ps1` Test〜D 差 0。
- **次の 1 手:** 利用者に報告し、テキストの 3D Layers を続けるか、設計書 v4 の P0 に戻るかを聞く。
- **気を付けること:** Fast (2D) のシェイプ・テキストでは、レイヤー自身の 2D の変形はエフェクトより先に掛かる（連続ラスタライズ）。
  `vector_probe.jsx` の `DUPLIX_VP_ONLY` で場合を絞れる。デバッグ用のログ（`dbg: Count class`・`dbg: text redirect`）が入っている（回数に上限あり）。


## 再開の手順（2026-11-27 20 時台、今の DupliX のシェイプ・テキスト対応と Fast の範囲に着手）

- **利用者の指示:** 設計書 v4 の 2.5 節を決定として反映（済み）。その前に、今の DupliX で、シェイプ・テキスト（ベクター）への対応、
  Fast (2D) でベクターのバッファの大きさが合わない件、自動拡張を先に進める。
- **調べたこと（`tests/ae/vector_probe.jsx`、結果 `build/test/vector_probe/sheet.png`）:**
  - 3D Layers: シェイプもテキストも複製されない（1 枚のまま）。フックは `BEE_AVLayer` の vtable だけに書いている。
    `BEE_VectorLayer`・`BEE_TextLayer` は別の vtable。フックするスロット（IsLayerType 79・Separately 217・Count 219・Rects 233）のうち、
    Vector は 79 を、Text は 79 と **219（Count、文字単位 3D 用）** を自前で持つ。217・233 は共通。Rasterize2DGraph（235）は両方とも別。
    243 番以降は両方とも vtable の外（ゴミ）。スクラッチパッドの `vt2.py`（pefile で vtable を比べる）。
  - Fast (2D): 位置がずれ、コンプの左上 4 分の 1 しか描かない。**ベクターのレイヤーでは、エフェクトの座標の原点がずれている**:
    シェイプの (0,0)（アンカー）がエフェクトの画素ではコンプの中央 (200,150)。DupliX はアンカー (0,0) をそのまま使い、
    出力の範囲が (0,0,200,150) になっていた。入力の絵はコンプの外の部分も含めて来ている（端の試験）。
- **(1) Fast の原点は直した（この節の commit）:** ベクター・テキストのレイヤーでは、エフェクトの画素 ＝ レイヤーの座標 − アンカー ＋ コンプの半分
  （アンカーを (17,−11) に動かすと入力の範囲が逆に動いた）。つまりアンカーはいつもコンプの半分の位置。`FastSetup` で
  `AEGP_GetLayerObjectType` が VECTOR / TEXT のときアンカーをコンプの半分にする。`tests/ae/vector_probe.jsx`＋`check_vector.py fm`
  （重心のずれ 0.1 px 以内・明るさの量 1% 以内）で shape・text・奇数のコンプ・アンカー移動・半分の解像度・端・プリコンプが PASS。
  `run_fast_mode.ps1` FAST ALL PASS（平面は変わらない）。
- **進め方（助言役と合意）:** (1) Fast の原点を直す（ベクター・テキストは「エフェクトの画素 ＝ レイヤーの座標 ＋ コンプの半分」の仮説を、
  奇数の大きさのコンプ・アンカーの移動・半分の解像度・テキスト・プリコンプで確かめる）。(2) 3D Layers でシェイプ（vtable ごとの元の関数、
  書く前にスロットの中身を確かめる、Optimize と M バッファーはまず切る）。(3) テキスト（文字単位 3D がオンなら AE に任せる）。
  (4) Fast の自動拡張（Rendering に Canvas: Comp / Auto、Disk ID 39）。(5) 既存の試験一式で平面が崩れていないことを確かめる。

## 再開の手順（2026-11-27 22 時台、v4 の未決に利用者が回答。Morph の合わせ方を説明して回答待ち）

- **利用者の回答:** マクロ 32・8・8 でよい。エディターは別の窓。初版は Windows だけ。ハリボテは捨てて作り直す。
  3D Layers でできないときの扱いは未回答（P0 の結果を見てから聞く）。v4 の 9.1 節と要望の文書に記録した。
- **質問への説明（Morph の合わせ方）:** 利用者は「形ごとに決まった数を対応させる」と理解していた。返事: 同じ数の形どうし
  （例: 格子 25 と円 25）なら v3 と同じく一対一でずれない。数が違う形どうし（格子 25 と正二十面体の頂点 12 など）のときだけ
  合わせ方が要る。数を共通にしたいときは 1 つのマクロを両方の数につなぐ。→ 既定の合わせ方の回答待ち。
- **利用者（22 時台）:** 「重なって集まる」は欲しい。「余りを縮めて消す」は Morph に限らず独立した手段でできるようにしたい（汎用性のため）。
- **返した案（回答待ち）:** (1) Morph の対応は 2 つ: Spread（既定、少ない方へ振り分けて重なって集まる）と One-to-one（番号どうし。
  相手のいない余りは Morph では動かず、そのまま残る）。Morph は自分では消さない。(2) 独立した Operator **Absorb**（吸い込み）:
  選んだ要素（Mask の Weight）を、行き先（同じ並びの残る要素のいちばん近いもの／別の形のいちばん近い要素／点のマクロ）へ動かしながら
  縮め・薄くし、100% で消える。Amount はマクロ・ENV・Stagger で動かせる。Morph の外でも使える（数を減らす、合体、ブラックホール）。
  (3) 「縮めて消す Morph」はプリセット（One-to-one の Morph ＋ 番号が相手の数以上を選ぶ Mask ＋ Absorb、量は同じマクロ）。
  Mask の範囲に「別の形の数」を読める入力を足す。
- **利用者:** 「重なって集まる」を Morph の機能にするのは汎用性的にどうか、それも含めてレビューさせてほしい。
  → v4 の 2.5 節に案 A（Morph に Spread / One-to-one、Absorb を独立）と案 B（Pair と Move To Target に分け、Morph も吸い込みも組み合わせ）を
  並べて書き、文脈なしのレビュー役（文書 2 つだけ）に判断させている。
- **レビューの結論（v4 の 2.5 節に記録）:** 「重なって集まる」は Morph の機能でよい（数をそろえる・ID を管理するのは Morph の役）。
  出力の数は形の数の最大に固定、ID は (Morph, i) で途中で替えない、One-to-one の相手なしは透明度でフェード。
  Pair と Move To Target は汎用の部品として出し、吸い込み（Absorb）はその組み合わせのプリセット。Nearest は Pair Time で決めてキャッシュ。
- **次の 1 手:** 利用者に分かりやすく伝えて決めてもらう。決まったら P0（別の窓からグラフデータを書き換えて Undo できるか）から始める。

## 再開の手順（2026-11-27 21 時台、シンセ風の設計書 v4 を書き、文脈なしのレビュー役に確かめさせている）

- **利用者の決定（11/27 21 時）:** シンセ風にする（中身はエディターの中だけ、AE にはマクロだけ）。案 C を置き換える。
- **済み:** `docs/design/複製プラグイン設計案_v4.md`（グラフ＝Generator・Operator・Modulator、マクロ 32＋色 8＋点 8、Layer 1〜8、
  Emitter は配置＋Stagger＋Motion＋Life＋Loop、v3 の Order・量・色・Pivot・Canvas・描き方を引き継ぐ、P0 の最大の要検証は
  別の窓からグラフデータ（Arbitrary Data）を書き換えて Undo できるか）。要望の文書に決定（改）を追記。v3 は経緯として残す。
- SDK で確かめたこと: `PF_UpdateParamUI` は名前を変えられる（見た目だけ、保存されない）。`AEGP_SetStreamName` は INDEXED_GROUP の子だけ。
  `AEGP_SetStreamValue` の注記は Arbitrary Data に使えないとも読める（要検証）。
- **レビュー（済み）:** 指摘 10 件（高 4: Motion の評価の範囲、固定の ID、ドラッグと取り消し、ピクセルを読む入力）をすべて v4 に反映（v4 の 10 節）。
- **次の 1 手:** 利用者に報告し、v4 の 9 節の未決 6 つを聞く。答えを待つ間に進められるのは P0（SDK の検証、特に別の窓からグラフデータを書き換えて Undo できるか）。

## 再開の手順（2026-11-27 20 時台、他の人の助言（Emitter・パス・シンセ風の操作）への判断を返した。利用者の方針待ち）

- **利用者（11/27 20 時）:** 「やはり Emitter は必要かもしれない」。他の人の書きかけの助言（配置を t の関数のパスとして考える、
  多面体などは「ジオメトリ → ジオメトリの並び」の連鎖、配置とパーティクルの接続は「0 秒の時点のパーティクル＝配置」、
  シンセの LFO / ENV のように外にパラメータを出し、残りは独自エディターの中だけ、ゲームエンジンも見る、Cavalry の Distribution 一覧）への判断を求められた。
- **返した判断（要点）:** (1) Emitter は「順に生まれる仕組み」を作らなくても、配置 ＋ 時刻のずらし（Stagger の Interval）＋ 出た後の動き
  （初速・重力・乱流を時刻の式で）の組み合わせで同じ見た目が作れる（原則「時刻と要素番号だけ」を守れる）。→ Modifier に「出た後の動き」を足す案。
  (2) 配置は「t で決まるもの（線・円・格子・球・パス）」と「点の並びを返すだけのもの（多面体・頂点・アルファから敷き詰め）」の 2 種類を持つ。
  (3) シンセ風（外に出すのは少数のつまみ、残りはエディターの中だけ）は案 B に近く、案 C の「全部の値にキーフレーム」と両立しない。
  利用者が決める分かれ道として出した。**設計書はまだ変えていない。**
- **次の 1 手:** 利用者の方針（特に (3)）を聞いてから、要望の文書と v3 を直し、文脈なしのレビュー役で確かめる。

## 再開の手順（2026-11-27 19 時台、利用者の決定で v3 を改訂し、文脈なしのレビュー役に 2 回目のレビューをさせている）

- **利用者の決定（11/27 19 時）:** 3D Layers も取り込む。エディターは自前で描く（Web の画面は重いので絶対に使わない）。
  **粒子と複製を分ける:** Emitter のように順に生まれる仕組みはやめ、最初から並んだものが時間とともに出現する形にする（Order が効く）。
  v3 の未決 2（Emitter の要素の割り当て）・3（Gather）は「よく分からない」との返事だったが、Emitter をやめたので不要になった。
- **済み:** v3 を改訂（Emitter・Emission・Age を外し、Modifier の Type **Reveal**＝時間で出現を追加。3D Layers と Fast の 2 つの描き方の
  できること・要検証の表、エディターは自前、実装の順番に 3D Layers の取り込み P5、枠は Shape 4・Modifier 12・Gradient 4・Source 8 で約 720）。
  要望の文書に決定を追記。
- **2 回目のレビュー（済み）:** 指摘 12 件（高 4）をすべて v3 に反映（v3 の 11 節に記録）。総評「3 つの決定は意図どおり。高の 4 件を直せば
  P0〜P2 は進めてよい」→ 直した（Order は要素ごとの値 q と段 s、Stagger に時間の出現を統合、量の掛け方の表、表示を切り替える選択は時間で変えない）。
- **次の 1 手:** 利用者に v3 の 10 節の未決 5 つを聞く（枠の数・エディターの入り口・Windows だけか・3D Layers でできないときの扱い・既定の描き方）。
  答えを待つあいだに進められるのは P0（SDK の検証）。利用者は IT・グラフィックの用語に詳しくないので、報告は用語を避けてやさしく書く。

## 再開の手順（2026-11-27 18 時半、要望の文書と設計書 v3 を書き、文脈なしのレビュー役に確かめさせている）

- **利用者の回答（11/27 18 時）:** (1) エディターは案 C（構造は独自データ、値はパラメータの枠）で進める。(2) 409 パラメータでも重くない。
  (3) 回転ノブはまだ消えていない。追加の要望: Grid のバッファの自動拡張、大きさを変えるときの中心（何かの要素を中心に）、透明度・色の合成、
  グラデーション（色の補間を何種類も）、Order に沿って透明度・色を掛け合わせる／色を変えていく。**今は実装せず、要望を文書にして設計書を見直す。
  見直しは文脈を最小にしたレビュー役のサブエージェントで確かめる。**
- **済み:** 隠す可能性のある角度の行（Angle Range・Twist・Latitude Range・Spread・Turbulence Evolution・M の Phase・Evolution・Rotation）を
  度の数値スライダーに替えた（ダイヤルが隠しても残るため。Global Rotation は隠さないので角度のまま）。**消えたかは利用者の確認待ち。**
  要望の文書 `docs/design/2026-11-27-wanted-features.md`、設計書 `docs/design/複製プラグイン設計案_v3.md`（枠＝プールの共通の仕組み、Shape 枠と
  連続 Morph、Emitter 枠、Appearance と Gradient 枠と Cascade、Pivot、Canvas Auto、エディター）。
- **レビュー（済み）:** 文書 3 つ（要望・v3・v2）だけを読ませた general-purpose のサブエージェントの指摘 12 件（高 3・中 6・低 3）を
  すべて v3 に反映した（v3 の 11 節に記録）。総評は「骨組みは進めてよい。Emitter と Count・時間で変えてはいけない値・Pivot の定義を P1 前に直す
  （直した）。3D の描き方は Appearance より先に決める」。
- **次の 1 手:** 利用者に v3 の 10 節の未決事項 5 つ（特に 2 の 3D の描き方）とノブが消えたかを聞く。答えが出たら P0（SDK の検証）から始める。

## 再開の手順（2026-11-27 18 時台、試用の報告 2 件を直した。設計の見直しについて利用者の判断待ち）

- **利用者の報告（11/27 18 時）と対応（この節の commit）:**
  1. 「非表示なのに回転ノブだけ見える」→ 隠すときに AEGP の HIDDEN に加えて `PF_UpdateParamUI` で `PF_PUI_INVISIBLE` も立て、
     切り替えた後は `PF_OutFlag_REFRESH_UI` で Effect Controls を描き直させる（`dx2_aegp.cpp` の `ApplyHidden`）。**直ったかは未確認（利用者に見てもらう）。**
  2. 「Grid Size を大きくすると複製物の大きさも変わる。何かを触ると間隔だけ変わる」→ 仮の描画がコピーを升目の 90% に合わせて縮めていたため
     （X だけ変えると Y の升目で大きさが決まり、間隔だけが変わって見えた）。コピーは素材の元の大きさ × 新しいパラメータ **Copy Scale**
     （入口の Random Seed の次、既定 100%）にした。Grid Size は位置だけ。試験 `copy width` を追加（8・32 bpc PASS）。
     パラメータが 1 つ増えて番号がずれた（プロトタイプなので可）。試験は名前で引くように直した。
- **利用者の質問（回答待ち）:** Emitter を複数作れないか。Morph で Polar → Sphere のように続けて変化できないか。設計を見直すかもしれない、
  独自エディターも考えている。→ 今の設計ではどちらもできない（Emission は 1 つ、Morph の行き先は 1 つ）。返答で「リストはすべて
  事前に確保した枠（プール）にする」案と、エディターの 3 案（AE のパラメータのまま Designer 窓／全部独自データ／構造は独自データ・値は
  パラメータの混合）を出した。**利用者が方針を決めるまで、中身の実装は進めない。**
- **次の 1 手:** 利用者の回答を聞く（ノブが消えたか、408 パラメータの重さ、設計の方針）。

## 再開の手順（2026-11-27 17 時台、DupliX 2 のハリボテのエフェクトコントロールを作った。利用者の試用待ち）

- **利用者の指示（11/27 16 時）:** 設計書（デスクトップの `duplixsekkei`）を取り込み、v2 を採用。入口はシンプルに、奥で細かく設定できる形。
  Custom UI で分かりやすく。まず中身は作らずハリボテのエフェクトコントロールを作る。複数の画像の読み込みは実装する。
  回答: 今の DupliX とは**別のエフェクト**として作る／画像は**コンプのレイヤー**を選ぶ／読み込んだ画像は**簡単な格子で実際に並べる**／複製元は**最大 8**。
- **済み（この節の commit）:**
  - 設計書 2 つを `docs/design/` に写した（v2 が採用、ChatGPT 案は下敷きとして保存）。
  - 新しいエフェクト「DupliX 2 (Prototype)」（マッチ名 `DupliX2`、`src/v2/`、`pwsh tools/build.ps1 -Target v2` → `dev_plugins\DupliX2.aex`）。
    パラメータ 408（Modifier 8 枠 × 39）。Custom UI 3 つ（Overview・Modifier List・Source List）。使わない行は隠す。
    中身があるのは Sources（8 つのレイヤー、Assign）と仮の格子（Count A × B、Grid Size X/Y）だけ。
    詳細・調べたもの・何が動くか: `docs/design/2026-11-27-v2-effect-controls.md`。
- **試験（この commit のソース）:** `tests/ae/v2_sources.jsx`＋`check_v2_sources.py` を 8・16・32 bpc で PASS（Source なし＝自分のレイヤー、
  Sequential、By A で逆順、Source 2 を None で詰まる）。既存 DupliX の smoke PASS（2 つのプラグインが並んで動く）。
  表示の切り替えはログで確認（最初の 1 回 407 行 約 10〜18 ms、その後は変わる行だけ 0.4 ms、最後の状態が Shape と一致）。
  **Custom UI の見た目とクリック（+ Add / ×）は未確認。** 隠したデスクトップの AE では PrintWindow で AE のパネルが写らない
  （`tests/ae/run_v2_ui.ps1` は場面を作るだけ）。利用者の AE で見てもらう。
- **次の 1 手:** 利用者に試用を頼み、Custom UI の見た目・クリック・パラメータの並びの感想を聞く。直したら、中身（設計 v2 の P1〜）へ進むか利用者に聞く。
- **気を付けること:**
  - プラグインのジャンクション `MediaCore\DupliX_dev` は 11/27 16:35 に利用者が作った（管理者の PowerShell で `New-Item -ItemType Junction`）。
    利用者の管理者 PowerShell では `cmd` が devkitPro の `C:\devkitPro\msys2\usr\bin\cmd` を指すので、`cmd /c mklink` は動かない。PowerShell の命令で案内する。
  - AE の `effect_ref` はコマンドごとに変わる。インスタンスの見分けはシーケンスデータの番号（`dx2_aegp.cpp` の `SeqData`）。
  - SmartRender では入力（`checkout_layer_pixels`）を出力（`checkout_output`）より先に取る。逆にすると 516 で失敗した。
  - 隠した行にはスクリプトから値を入れられない（AE の制限）。試験は `$.setenv("DUPLIX2_SHOW_ALL","1")` で全部の行を出してから値を入れる。
  - 32 bpc の `saveFrameToPng` は効果なしの赤い平面でも (25,0,0) になる。試験は基準の `ref_red.png` との比で判定する。
  - パラメータの番号＝Disk ID。出荷したら並べ替え・挿入は不可（後ろに足すだけ）。今はプロトタイプなので変えてよい。
  - Custom UI の高さは PARAMS_SETUP で固定。いちばん狭い Effect Controls（ログで 240 px・12 pt）に合わせた（Overview 124、
    Modifier List 88＝3 列 × 3 行、Source List 94＝2 列 × 4 行）。
  - 「+ Add Modifier」とチップの × は、DO_CLICK で `params[]` の Type の値を変えて `change_flags` を立てる形（未確認）。
    利用者から「押しても何も起きない」と言われたら、Remove Source と同じく表示してから `AEGP_SetStreamValue` を取り消しのまとまりの中で使う形に直す。

## 再開の手順（2026-11-27 15 時台、名前を DupliX に統一・パラメータを英語の定番の並びに。利用者への報告待ち）

- **方針（利用者、11/27 午後）:** 軽量化は一旦止め、プラグインとしての機能を詰める。名前は DupliX（旧名はリポジトリから排除、git 履歴は除く）、
  パラメータは英語、補足の文言なし、状態の欄なし。詳細は `docs/experiments/2026-11-27-dev-log.md` の 13 節。
- **今の状態:** この節の commit まで push 済み。`dev_plugins\DupliX.aex` はこの commit のソースのビルド（Release）。GitHub のリポジトリ名は
  DupliX に変更済み（remote も更新）。
- **利用者の作業（待ち）:** Claude Code を閉じて、このリポジトリのフォルダー（`A:\MyProjects\After Effects\Plugin Project\` の下の旧名のもの）を
  `DupliX` に名前を変える。その後、管理者の PowerShell でプラグインのジャンクションを付け直す（`MediaCore` の下の旧名のジャンクションを消し、
  `cmd /c mklink /J "C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\DupliX_dev" "A:\MyProjects\After Effects\Plugin Project\DupliX\dev_plugins"`）。
  管理者の権限が要るため Claude からは作れなかった。メモリーは新しいフォルダー用の場所にも写してある。
- **試験（この commit のソース）:** 単体 81・GPU、smoke、TRANSFER、FAST、REF、UI、EDIT、MERGE、AS-IS、ORBIT ALL PASS、Test〜D 各 8 フレーム差 0。
- **次の 1 手:** 利用者の反応を聞き、機能を詰める作業に入る（何を詰めるかは利用者の指示を待つ）。
- **気を付けること:** パラメータの Disk ID は変えない（`duplix_params.h`。1 と 15 は欠番）。試験のスクリプトはパラメータを内部名 "DupliX-00NN"
  （NN＝Disk ID）で参照する。古い内部 ID の aep は `tools/aep_matchname.py <in> <out> <旧 ID> DupliX` で変換できる。

## 再開の手順（2026-11-27 14 時前、M バッファーの置き換えを完了。利用者への報告・試用のお願い）

- **今の状態:** この節の commit まで push 済み。`dev_plugins\DupliX.aex` はこの commit のソースのビルド（Release）。
  M バッファー（塊）へのコピーの描画を DupliX が行い（既定で有効、`DUPLIX_MBUF=0` で AE に戻す）、面を溜めて解決の直前に GPU で
  まとめて計算する（`DUPLIX_MBUF_BATCH=0` で面ごと）。詳細は `docs/experiments/2026-11-27-dev-log.md` の 12 節。
- **成果:** Test D に大きい画像 380 枚 1 フレーム 1266 → 207 ms（span）、bigS3271 の塊 230 → 125 ms。断片は AE と 1 つずつ一致を確認済み。
- **試験（この commit のソース）:** 単体 81・GPU ALL PASS（新しい `gpu_ae_mbuf`）、MBUF 0/1 の絵の比較（bigS3271・花の Test C 3271・Test D 380・
  Test D 大きい画像）一致、`run_big_merge.ps1` 12 フレーム一致・`-Nest` 一致、EDIT・MERGE ALL PASS、`run_user_merge.ps1` Test〜D 各 8 フレーム差 0。
  試験一式のスクリプトはスクラッチパッドの `suite.sh`・`mbcmp.sh`（消えていたら上の試験を順に回すだけ。`run_big_merge` は `-Nest` の前に比べること）。
- **次の 1 手:** 利用者に報告し、試用を頼む（Test D に大きい画像など）。続けるなら、11/27 8 時の節の次の 1 手の 2〜4
  （辺の作成の残り・sliver・計時）か、開発記録 12 節の最後の「塊の前に組のコピーを流す 28 回」（解決も DupliX の計算にする必要がある）。
- **気を付けること:** M バッファーの vtable（+24 解決・+0 削除）を書き換えている（`MbufHookVt`）。Advanced3D の版が変わったら、3 つの挿入関数の
  RVA（`kMbufInsertRva`）と断片の形を確かめ直す。

## 再開の手順（2026-11-27 13 時、M バッファーの行の処理の置き換えの途中。利用者「一旦中断してください」）

- **今の状態:** この節の commit で、M バッファー（塊）へのコピーの描画を DupliX が行う版を push 済み（既定で有効、`DUPLIX_MBUF=0` で AE に戻す）。
  `dev_plugins\DupliX.aex` はこの commit のソースのビルド。全体の試験（単体・GPU・EDIT・MERGE・Test〜D）は**まだ回していない**。
- **分かったこと:**
  - 計測（`DUPLIX_A3D_TIME=2` の `mbuf:` の行）: Test D に大きい画像 380 枚は塊の描画 約 1.2 秒のうち、AE の行の処理が約 1.1 秒（81 万画素）、
    bigS3271 は塊 230 ms のうち行の処理 125 ms・塊の前に流す GPU の仕事 100 ms・解決 3〜4 ms。
  - 実際の経路は `sub_1800477F0` ではなく、M バッファーの仮想関数 +8 が真の `sub_18004D870` → `sub_180046A90` → 挿入 `sub_180021710`
    （32 バイトの断片に dWx・dWy）。3 つの行の処理（477F0・46A90・48500）は計算が同じで、挿入だけ違う（`bee_hooks.cpp` の `kMbufInsertRva` の注釈）。
  - 奥行き: 2 画素の組の 1 枚目は `float(f0+W)+off`（float）、2 枚目と最後の 1 画素は double で足して 1 回丸める。off＝`((-dWx*0.5f)-0.5f*dWy)+params[0xA8]`。
- **実装:** `core/ae_span_impl.h` の `MbufOff`・`MbufDepth`・`MbufPairFirst`、`AeSpanCore::zoff`（旧 pad）、`AeMbufSpan8`（CPU）、
  GPU `AeMbufSpans`（`ae_span.cu`）と `GpuAeMbufSpans`（`gpu_raster.cpp`、初期化とテクスチャを `AeInit`・`AeTexture` に切り出した）、
  フック `OurIterate` の種類 2 → GPU で断片を計算 → AE の反復の中で `OurMbRow` が AE の挿入関数で入れる（スレッド番号は AE のもの）。
  診断: `DUPLIX_MBUF=2`（AE が描き、入った断片を 1 つずつ DupliX の計算と比べる）、`=3`（CPU で計算）。
- **確かめた結果:** `DUPLIX_MBUF=2` で bigS3271 53 万画素・Test D 大きい画像 約 240 万画素が全部一致（違ったのは AE の反復が 517＝中断を
  返した描画だけ）。Test D 大きい画像で MBUF=0 と 1 の絵が一致（CPU 版・GPU 版とも）。速さ: Test D 大きい画像 1 フレーム 1216 → 465 ms
  （span）。GPU は 1 面ずつ呼んでいて 376 回で約 290 ms。
- **次の 1 手:** (1) 全体の試験（単体・GPU・EDIT・MERGE、`run_user_merge.ps1` Test〜D、`run_big_merge.ps1`、bigS3271 の MBUF 0/1 比較、花の Test C・D）。
  比較の道具はスクラッチパッドの `mbcmp.sh`（消えていたら: MBUF=0 と 1 で `run_preview.ps1` を回し `cmp_dirs.py` で比べるだけ）。
  (2) GPU の呼び出しを 1 面ずつ → まとめる（溜めて、AE が同じ M バッファーに描く前・解決の前に流す。同じ奥行きの断片は入れた順が効くので面の順を保つ）。
  (3) 開発記録（11/27 の 12 節）に今回の内容を書く。トラブル: 検査で「AE は入れていない」と出たのは中断された描画だった。

## 再開の手順（2026-11-27 14 時、Test E の件を閉じた。利用者がセッションをクリア）

- **今の状態:** commit 9ac54e4 まで push 済み。作業ツリーに残る変更はなし（`git status` に出る他のファイルは改行コードの見かけだけ。
  `git diff --stat` は空）。`dev_plugins\DupliX.aex` は 63bb832 以降のソースのビルド（Release。その後の commit は試験と記録だけ）。
- **この日（11/27）の成果（詳細は `docs/experiments/2026-11-27-dev-log.md` の 6〜10 節）:**
  - 大きい画像の軽量化（GPU のテクスチャのキー、cull の自動、1 画素 1 スレッド、組にならないコピーのまとめ描き、テクスチャの写しの使い回し）。
    Test C に 1920x1080 を入れた画面の経路: 380 枚 1205 → 約 90 ms、3271 枚の重なり 約 2 秒 → 約 0.75 秒、花 380 枚 86 → 56 ms。
  - CPU: 計画の対の判定 290 → 27 ms、AE の並べ替えの辺の作成 633 → 約 360 ms（bigS3271）。
  - 不具合の修正: 組の中の塊（循環する重なり）を AE の M バッファーで描く（bigS3271 のオン≠オフ 9〜12 画素を解消）。3D で交わるコピーも
    組に入れる（`pv_cross200` でオン＝オフ差 0 を確認）。`DrawGroup` で +129 の面の線を溜めた GPU の仕事より先に描いていたのを直した。
- **Test E の報告（閉じた、開発記録 10 節）:** 「ワールド変形の Y 回転 +6 と +7 でコピーの描画順序が変わる」。書き出し・画面の経路とも
  軽量化オン＝オフ差 0。6° の状態から作った本物のレイヤーの双子をヌルで +1° 回しても DupliX の 7° と同じ絵（最大 2/255）＝AE 自身の
  描き方。**利用者も確認済み（11/27 14 時）。**
- **試験（最終版のソース）:** 単体 81 件（対の判定は AE と 200 万通り一致）・GPU ALL PASS、EDIT・MERGE（AE の辺と突き合わせ）ALL PASS、
  `run_user_merge.ps1` Test〜D 各 8 フレーム差 0、`run_big_merge.ps1`（大きい画像・切り抜き 50%）一致、入れ子 `-Nest` 一致。
- **次の 1 手（利用者の指示「軽量化も不具合も全部やる。質問せず計測で判断」）:** 下の 8 時の節の「次の 1 手」の 1 から
  （M バッファーの行の処理 `sub_1800477F0` の標本化を DupliX の計算に置き換える。解析の入口は開発記録 9 節の最後）。
  利用者に試用を頼むかどうかは、この作業の区切りで決める。
- **新しい試験の道具（この日）:** `tests/ae/run_big_merge.ps1`（`-Nest` で入れ子）、`tests/ae/make_big_png.py`（4 番目の引数で切り抜き）、
  `preview_setup.jsx` の `DUPLIX_PV_BIG`・`DUPLIX_PV_BIG_MERGE`・`DUPLIX_PV_NEST`・`DUPLIX_PV_CROSS`・`DUPLIX_PV_SET`、
  `user_merge.jsx` の `DUPLIX_UM_SET`、`orbit_cut.jsx` の `DUPLIX_OC_REF=dupe`・`DUPLIX_OC_KEEPCAM`・`DUPLIX_OC_SET`・
  `DUPLIX_OC_PIVOT`・`DUPLIX_OC_PIVOT_RY_<変種>`。診断: `DUPLIX_PLAN_MEETS`・`DUPLIX_EDGE_THREADS`・`DUPLIX_BATCH_SINGLES=0`・
  `DUPLIX_GPU_PIXELS=0`・`DUPLIX_GPU_CULL=0/1`・`DUPLIX_GROUP_CLUSTER=1`。双子の一覧は計画の書き出し（`DUPLIX_MERGE_DUMP`）の
  4 隅から作れる（スクラッチパッドの `twins_from_plan.py`。消えていたら作り直す: 4 隅から位置＝c0 + アンカー分、Y 回転＝atan2(−u.z, u.x)）。
- **気を付けること:** 画面の経路の速さは span で見る（単独のコピーは `sub_180010E00` の直前でまとめて描くので `in-plugin` に入らない）。
  大きい組は `timing:` の行に組の描画が入らない（`group time:` で見る）。AE を動かす試験は 1 つずつ。

## 再開の手順（2026-11-27 8 時、大きい画像の軽量化と塊の画素不一致の修正。利用者の指示で残りの速さを続ける）

- **利用者の試用の結果（11/27 4 時）:** 「かなり速くなった。見た目の問題はなし。ただ 1920x1080 など大きな画像を複製対象にすると重い。
  GPU ならもっと速くできるはず」。NVIDIA の電源管理は変更済み、ほかのプロセスは GPU を使っていない。
- **済み（この節の commit、詳細は `docs/experiments/2026-11-27-dev-log.md` の 6 節）:**
  1. テクスチャのキーを共有の master の通し番号に（コピーごとの 8 MB の比較をなくした）。
  2. cull（不透明な画素の下を飛ばす、完全一致）を、テクスチャの 10% 以上が不透明なら自動でオン（切り抜き 50% でも 352 → 19 ms）。
     画素ごとと 8 画素ごとのカーネルは、AE の中で bigS・bigL の 380 枚・3271 枚とも絵が一致。
  3. GPU をスレッド 1 本で 1 画素に（`AeSpanPixels`、既定）。
  → Test C に 1920x1080 を入れた場合: 380 枚 1205 → 268 ms、3271 枚の組の描画 約 900 → 60〜78 ms。花は変わらず。
- **試験（この版）:** 単体 81 件・GPU（不透明 960x540 を追加、cull・カーネルの 4 通り）ALL PASS、EDIT ALL PASS、MERGE ALL PASS、
  `run_user_merge.ps1` で Test・A・B・C・D 各 8 フレーム オン＝オフ差 0、bigS380・bigL380 はオン＝オフ一致。
- **大きい画像の試験の仕方:** `python tests/ae/make_big_png.py build/test/big/big1920.png` のあと
  `$env:DUPLIX_PV_COUNTS='380,3271'; $env:DUPLIX_PV_BIG=<png の絶対パス、/ 区切り>; pwsh tests/ae/run_preview.ps1 -Aep "tests/ae/user/DupliX Test C.aep" -Layer "flower_blue.png" -Frames 5 -IntervalMs 3000 -Variants "pv_bigS380,pv_bigL380,pv_bigS3271"`。
  `DUPLIX_PV_BIG_MERGE=0` で軽量化オフの基準。**注意:** bigS3271 などの大きい組は `timing:` の行が組の描画より前に出て、
  組の時間が入らない。`group time:`（`DUPLIX_A3D_TIME=2`）で見る。
- **利用者の回答（11/27 7 時）:** 「軽量化できるなら全部やる。枚数などは計測で判断できるので聞く意味はない。不具合も速さも全部やる。
  画素不一致は直せる場合絶対直す」。→ 質問せず、不具合を先に、次に大きい順に進める。
- **済み（7 時台）:** bigS3271 のオン≠オフ 9〜12 画素を修正（組の中の塊を AE の M バッファーで描く。開発記録 7 節）。
  `tests/ae/run_big_merge.ps1`（大きい画像のオン＝オフ比較、既定の間隔 15 秒）を追加。全試験合格（開発記録 7 節）。
- **済み（8〜9 時台、開発記録 8 節）:** 計画の対の判定 290 → 27 ms、辺の作成 633 → 343〜388 ms（bigS3271）。全試験合格。
- **済み（10〜11 時台、開発記録 9 節）:** 組にならないコピーをフレームの中で溜めてまとめて GPU で描く（bigS380 268 → 約 90 ms、
  花 380 枚 86 → 56 ms。span で比べる）。テクスチャの写しの使い回し（bigS3271 の描画 320 → 約 240 ms）。交わるコピーを組に入れる
  変更を `pv_cross200` で確認（交わる対 5537、200 枚の塊、オン＝オフ差 0）。全試験合格。
- **次の 1 手（順に全部やる）:**
  1. 塊（組の中の塊も、Test D のように家と循環する大きな塊も）を M バッファーに描く AE の行の処理 `sub_1800477F0` の、標本化の部分を
     DupliX の計算に置き換える（断片の挿入 `sub_180021A40` と解決は AE のまま）。開発記録 9 節の最後に解析の入口。Test D に大きい画像
     380 枚で 1 フレーム約 1.1 秒（AE だけで描いても同じ）。完全一致の確認には、M バッファーの断片を取り込んで比べる試験が要る。
  2. 辺の作成の残り（対の判定 約 220 ms・AE の確保関数で辺 130 万本 約 90 ms）。
  3. 手前・奥の計画の「sliver」（画面上でわずかに離れた対）で 380 枚が組にならない件。AE の並べ替えが順序の決まらない面をどう並べるかの解析が要る。
  4. 計時: 大きい組で `timing:` の行に組の描画が入らない件。
  - 標本化は `DUPLIX_A3D_PROF=2`（描画の糸を常に。待ちも入る）、名前の解決はスクラッチパッドの `sym.py`（消えていたら作り直す。
    dbghelp で `dev_plugins\DupliX.pdb`）。辺の作成のスレッド数は `DUPLIX_EDGE_THREADS`（診断用）。

## 再開の手順（2026-11-27 1 時、軽量化の続き。利用者がセッションをクリアし、そのあと自分で試す）

- **今の状態:** commit 30e09ec まで push 済み、作業ツリーはきれい。`dev_plugins\DupliX.aex` はこの commit のビルド（Release）。
  利用者は「セッションをクリアして、テストはその後やります」と言った。**再開したら、利用者の試用の結果（画面での速さ・見た目）を聞いてから次へ進む。**
- **この日の流れ（詳細は `docs/experiments/2026-11-27-dev-log.md`）:**
  1. Test D の「切れ目が動く」報告は AE 自身の描き方と分かり、利用者の判断で閉じた（下の 0 時の節）。
  2. 11/26 の最後の版（b0c63d7）の未確認の試験をすべて回して合格。
  3. GPU のクロックの差（待機 345 MHz と最大 2100 MHz）でタイル描きが 13〜17 ms と 78〜92 ms に分かれる。フレームの始めに空回りの処理で温める案は、AE の中では描画まで約 100 ms しかなく安定しないので入れなかった（コードは戻した）。利用者には NVIDIA コントロールパネルで AfterFX.exe の「電源管理モード」を「パフォーマンス最大化を優先」にする案内をした（したかどうかは未確認）。
  4. 組の描画の「AE の行の前の準備」を 40 → 10〜15 ms（Test C）に縮めた。原因は DupliX 側: (a) 診断用の環境変数をコピーごとに読んでいた → `src/hook/env_cache.h` の `EnvCache`（250 ms 覚え置き。試験の `$.setenv` も反映される）、(b) `IsMine` → `LayerIsLive` が comp の描画方式を BEE に問うたびに AE の排他ロックを取る（コピーごとに約 6 回）→ 描画の回の中のフックだけ `IsMineInPass`（回ごとにレイヤーの答えを覚え、`ClosePass` で捨てる。UI や Count・Rects は毎回確かめる）。
- **確かめた試験（30e09ec）:** EDIT ALL PASS（`run_preview.ps1 -Aep "tests/ae/user/DupliX Test A.aep" -Layer "flower_blue.png" -Variants "pv_anim_on100,pv_on20" -Edit 5`）、MERGE ALL PASS（`$env:DUPLIX_GRAPH_CHECK=1; run_merge.ps1 -Mfrs OFF,ON -CompareDraft -GpuDraw -ForceD "200,150,300"`）、`run_user_merge.ps1` で Test・A・B・C・D 各 8 フレーム オン＝オフ差 0、単体 81・GPU ALL PASS（core と GPU はこの日の最終版で変えていない）。
- **今の内訳（Test C、3271 枚、画面の経路、1 フレーム、CPU）:** 計画 約 25、AE の並べ替え 約 33（うち辺の作成 25〜35）、面の作成 約 6、組の描画の準備 10〜15、行の処理 約 4、GPU の前の CPU 約 22（並べ替え 5、区間のまとめ 16）→ 約 100 ms。GPU はクロックが上がっていれば 13〜17 ms。高速モードは同じ条件で 55〜145 ms（ぶれ大）。
- **気を付けること（計測）:**
  - 計測の時、AE 以外のプロセス（CUDA を使う python.exe と Android エミュレーター MuMu Player）が GPU を 100% 使っていて、GPU の段で 1〜2 ms の処理が 230〜285 ms 待たされる回があった。利用者に伝え済み。GPU の時間を測る前に `nvidia-smi` で GPU の使用率と使っているプロセスを確かめる。利用者のプロセスは止めない。
  - 計測: `$env:DUPLIX_PV_COUNTS='3271'; $env:DUPLIX_A3D_TIME='2'; pwsh tests/ae/run_preview.ps1 -Aep "tests/ae/user/DupliX Test C.aep" -Layer "flower_blue.png" -Frames 8 -IntervalMs 3000 -Variants "pv_fast3271,pv_on3271"` → `build\test\preview\plugin.log` の `group time:`・`graph time:`。GPU の段ごとは `DUPLIX_GPU_KTIME=1`（`gpu ae span` の行）。
  - どこで時間を使っているかは `DUPLIX_A3D_PROF=4`（組の描画の中を標本化、`DUPLIX_A3D_PROF_N=300` で少ない標本でも出る）。番地 `DupliX.aex+xxxx` は PDB（`dev_plugins\DupliX.pdb`）を dbghelp で引いて関数名と行に直す（Python の ctypes で `SymLoadModuleEx`・`SymFromAddr`・`SymGetLineFromAddr64`。スクラッチパッドの `sym.py` は消えているかもしれないので作り直す）。
  - AE を動かす試験は 1 つずつ（`run_jsx.ps1` は AE が動いていると止まる）。長いものは裏で回す。
- **次の 1 手:** 利用者の試用の結果を聞く。続けるなら CPU の残りの大きい順に: (1) 並べ替えの辺の作成（`HookBuildEdges`・`BuildEdgesFast`、25〜35 ms。対の判定は `core/ae_pair` で 8 スレッド済み）、(2) 計画の対の判定（`BuildIsolatedGroupPlan`、約 17 ms）、(3) GPU の前の区間のまとめ（`gpu_raster.cpp` の `AeSpansImpl`、約 16 ms）。どれも同じ絵のまま（完全一致は必須）。11/26 22:00 の節の (2)「AE の面ごとの処理の置き換え」は、準備が 10〜15 ms まで縮んだので優先度は下がった。

## 再開の手順（2026-11-27 0 時、Test D の「切れ目が動く」の調査を終えて利用者への報告・判断待ち）

- **結論:** DupliX の不具合ではない。AE Classic 3D 自身の描き方（本物のレイヤーでも同じ）。下の節の 1〜5 に加えて:
  6. 双子の枚数で絞った（判定は 40° で窓の上の青い画素数、正しくは 0）: 家と重なる 32 枚だけ 0、＋残りの前半（計 345 枚）1090、＋後半（346 枚）0、前半の 4 分の 1（各 110 枚）6〜12、4 分の 1 を 2 つずつ（6 通り、各 188 枚）285〜311、全部 1090。→ 特定の面ではなく、塊に入る面の数が増えるほど崩れる。
  7. 利用者の設定（軽量化オン、`dxon`）も 40/50/60/75° で軽量化オフと画素差 0、双子と ≤2/255。
  8. AE の中の仕組み（調べかけ）: 循環の塊は `sub_180013470` が M バッファー（`SY_MBufferDraftX`、画素ごとの断片の一覧、奥行きは単精度）に描き、`sub_1800477F0` が行の断片を `sub_180021A40` で入れる。奥行きは行の始まりの値に傾きを足す形に見えるが確かめていない。原因は未確定。
- **目で確かめる aep:** `build/test/orbit_cut_eye/DupliX Test D twins.aep`（comp `oc_twin`＝DupliX を切って双子 659 枚、`oc_twin317`＝家と双子 317 だけ、`oc_dx`・`oc_dxon`＝DupliX の軽量化オフ・オン。軌道ヌルは 75°）。
- **利用者の判断（11/27 0 時）: (a) 今のまま（AE と同じ絵）。気にしない。** この件は閉じた。軽量化の作業（下の 22:00 の節の「次の 1 手」）に戻る。

## 再開の手順（2026-11-26 23:10、利用者の報告「Test D で切れ目が動く」を調査中・利用者の指示で中断、11/27 0:02 に再開の予約）

- **報告:** `C:\Users\yakuruto\Desktop\DupliX Test D.aep`（写しは `tests/ae/user/DupliX Test D.aep`、659 枚、回転の範囲 180）で軌道ヌルの Y 回転を 40〜75° に動かすと、家の左上の青い花の切れ目が変わる。通常モードでも同じ。利用者は本物のレイヤー（レイヤー 7）では動かないと確認し、サブレイヤーの配置かカメラの計算の誤りを疑っている。
- **分かったこと:**
  1. AE がコピーに使う世界行列は DupliX の計算（コピー × 世界の変形 × レイヤー）とビット一致（`DUPLIX_XF_CHECK=1`）。カメラに依存しない。
  2. レイヤー 7 はどのコピーとも行列が一致しない（いちばん近いコピー 317 でも 170 px 以上ずれる。手で近くに置いた別の面）。
  3. 全 659 コピーと同じ世界行列の本物のレイヤー（双子）を作って書き出すと、40/50/60/75° で DupliX 通常モードと画素差 ≤2/255（オイラー角の丸め）。切れ目も同じ。→ **サブレイヤーの配置の誤りではない。本物のレイヤーでも同じ絵になる。**
  4. 窓の上の切れた花はコピー 317。4 隅の z は 241.5 / 39.1 / −4.4 / 198.0（家は z=0、カメラは −z 側）＝ほぼ全部が家の奥にあり、正しくは隠れるはず。家と 3D で交わるコピーは 317 だけ（交線は花の角の約 23 px）。
  5. 双子を 317 だけ・317 と画面で重なる 4 枚（181,317,457,618）・家と画面で重なる 32 枚にすると、どれも正しく隠れる。659 枚全部のときだけ大部分が手前に出て切れ目が動く。→ AE 自身が大きな循環の塊を描くときの崩れとみられる（未確定）。
- **次の 1 手:** 双子を半分ずつにして、どの面の追加で崩れるか絞る。AE の塊の描画（記録の解析ノート）を確かめ、なぜ崩れるかを説明できるようにする。そのあと利用者に報告（DupliX は本物のレイヤーと同じ。直すなら AE と違う絵にすることになるので方針は利用者が決める）。
- **道具:** `tests/ae/orbit_cut.jsx`（`DUPLIX_USER_AEP`、`DUPLIX_OC_DEGS`、`DUPLIX_OC_VARIANTS`=dx/dxon/real/twin、`DUPLIX_OC_TWINS`、`DUPLIX_OC_TWIN_ONLY`=コピー番号の並び）。双子の値は `build/test/orbit_cut_in/twins.txt`（"px py pz rx ry rz"、コピー順）。計算のスクリプト（コピーの行列・双子・カメラの投影）はスクラッチパッドにあったので消えていたら作り直す: Test D の DupliX は種 370、位置の範囲 1760/3860/8430、回転の範囲 180、世界の回転 (8,−25,62)、アンカー (108.5,108)、レイヤー位置 (960,540,0)。カメラは軌道ヌル（位置 960,540,0、拡大 241%、回転 X15 Z−14）の子で (0,0,−2880)、ズーム 2880。
- **気を付けること:** `run_jsx.ps1` で開いた写しは AE が保存するので、毎回 `tests/ae/user` から写し直す。作業ツリーには前の節の未 commit の変更（`merge_plan` など）が残っている（この節の commit には含めていない）。

## 再開の手順（2026-11-26 22:00、利用枠の見張り 70% で中断。利用者「進めてください」＝ (1) 手軽なもの → (2) AE の面ごとの処理の置き換え → (3) GPU の近道）

- **済み（この節の commit）:** (1) の一部。計画の対の判定を 8 スレッドに（`BuildIsolatedGroupPlan`、5000 枚 118 → 47 ms、Test C 45 → 25 ms）。行の受け取りで AE の進み具合の通知をコピーごと 1 回に（21 → 5.5 ms）。GPU のタイル描きで、各 8 画素が触れる区間だけを共有メモリのビット集合で選ぶ（AE の順のまま、Test C の再生でタイル 23 → 13 ms）。GPU の前の CPU の準備（行の区間のまとめ）を 8 スレッドに（14 → 5 ms）。不透明な画素の下を飛ばす描き方（`DUPLIX_GPU_CULL=1`）は書いたが Test C では遅くなる（縮小で平均した不透明度が 255 になりにくい）ので既定はオフ。
- **分かったこと:** GPU は待機中 345 MHz（最大 2100）で、短い処理の始まりは遅い。AE の中で GPU の時間が 22〜154 ms とぶれた主因（利用者が別の GPU を使うアプリも終了した）。利用者に NVIDIA コントロールパネルで AfterFX.exe の電源管理を「パフォーマンス最大化を優先」にする案内をする。
- **確かめた試験:** MERGE ALL PASS（辺の突き合わせ 392 回 差 0。CPU の準備の並列化の前の版）、GPU の試験（`gpu_ae_batch`、Test C の再生）差 0、単体 81 合格、画面の経路の Test C 3271・380 枚 オン＝オフ差 0。**未:** 最後の版（CPU の準備の並列化）での MERGE と利用者のシーン 4 つ。
- **次の 1 手:** 上の未確認の試験 → (2) AE の面ごとの処理（行の区間の作成 `sub_1800117A0`、コピーごとの状態 `sub_18005D6C0`、並べ替えのたどり）を DupliX に置き換える。検証は AE の関数を試験の中で直接呼んで比べる（`ae_pair` と同じ）。
- 計測: Test C の速さは画面の経路で 3271 枚 約 240〜340 ms（ぶれ大）、高速モード 55〜130 ms。GPU の段ごと `DUPLIX_GPU_KTIME=1`、CPU の準備の段ごとも同じ変数で出る。

## 再開の手順（2026-11-26 21:40、根本的な軽量化の手段を調査・利用者の判断待ち）

- 利用者「高速モードは自前の描画で速く絵もほぼ同じなのに、軽量化は手間がかかりすぎる。もっとクリティカルな最適化はないか。文脈を渡さない調査役なども使って模索して」。
- **測った事実:** Test C の本物の 1 フレーム（2359 枚、区間 19.7 万本、延べ 1290 万画素）を AE なしで再生すると GPU の描画は約 21 ms（`DUPLIX_GPU_KTIME=1`: 転送 1、歩み 1.3、タイル 21）。範囲平均をやめても変わらない（`DUPLIX_REPLAY_Q0=1` で 25 ms）→ 画素の計算は決め手ではない。AE の中では同じ処理が約 100 ms。ほかに CPU が約 240 ms。
- **調査役（文脈なし）の結論:** 完全一致に本当に要るのは範囲平均と対の判定だけで、残りは AE の仕組みに乗っている設計の手間。完全一致のままで 3271 枚 60〜100 ms 前後が見込める。順位: (1) 手軽なもの: AE の中で GPU が 2 倍遅い理由の計測、計画と並べ替えで同じ対を 2 回調べている分の共有、残り約 90 ms の CPU の内訳、(2) 範囲平均の確かめつきの近道（整数の累積表＋誤差の上限で同じバイトになると保証できるときだけ、だめなら今の計算）と、標本化と合成の分離・不透明な画素の上書きで下を飛ばす（完全一致のまま）、(3) 行の区間の作成・コピーごとの状態・並べ替えのたどりを DupliX に置き換え、AE の面ごとの処理を通さない（AE の関数を試験で直接呼んで比べる方式で検証）。
- **次の 1 手:** 利用者に報告して、(1) → (3) → (2) の順で進めてよいか確かめる。**回答待ち。**

## 再開の手順（2026-11-26 21:00、置き換えの第 1 段を実装・利用者への報告待ち）

- **今どこまで（この節の commit で push 済み）:** 利用者「置き換えに進んでください」「AE 上で書き出して確認するのは非効率。すり替える関数をシミュレーションで検証できないか」。詳細は記録 8d 節「置き換え」。(1) AE の対の判定を `core/ae_pair` に書き写し、単体テストで AE の `Advanced3D.aex` を読み込んで元の関数と直接比べる（20 万通り、200 万通りも差 0）。8 スレッド。並べ替え 250 → 40 ms。(2) テクスチャを複製しない（`AEGP_DisposeTexture` のフック）。(3) GPU のタイル描き、AE なしの検証（`gpu_ae_batch`、`gpu_ae_replay` と `DUPLIX_BATCH_DUMP`）。(4) レイヤーごとの時間が均等に見えるのは AE のプロファイラーの作り（合成の時間をレイヤー数で割る）で直さない。
- **結果:** Test C 3271 枚 634 → 約 340 ms、380 枚 290 → 171 ms（高速モード 55 / 8 ms）。
- **確かめた試験:** MERGE ALL PASS（辺の突き合わせ 392 回 差 0）、EDIT ALL PASS、Test A・B・C・Test 各 8 フレーム差 0、単体 81・GPU（新しい 2 つを含む）・REF・UI 合格。`dev_plugins\DupliX.aex` は最新。
- **次の 1 手:** 利用者に報告。残り（3271 枚）: 描画 約 200（GPU 約 100、AE の描画の準備 約 50）、計画 約 50、並べ替え 約 45。次の置き換えの候補は AE の描画の準備（多角形から行の区間を作る部分）。これも AE の関数を試験の中で直接呼んで比べる形で検証できる見込み。**回答待ち。**
- 再生の検証: `$env:DUPLIX_BATCH_REPLAY="...\build\test\batch_testc.bin"; build\gpu\gpu_test.exe`（記録は `DUPLIX_BATCH_DUMP` で 10 万区間以上の 1 回目）。

## 再開の手順（2026-11-26 19:50、1〜3 を実装・利用者への報告と相談待ち）

- **今どこまで（この節の commit で push 済み）:** 1 GPU のまとめ描き、2 計画、3 AE の並べ替えの辺の作り方（`HookBuildEdges`、四角が重なる対だけ AE の判定を呼ぶ）。詳細と数字は記録 8d 節「残りの時間を縮める」。Test C 3271 枚 軽量化オン 634 → 約 540〜580 ms（高速モード 55 ms）。
- **確かめた試験:** MERGE ALL PASS（`DUPLIX_GRAPH_CHECK=1` で辺の突き合わせ 392 回 不一致 0）、EDIT ALL PASS、利用者のシーン Test A・B・C・Test 各 8 フレーム差 0、単体 80・GPU・REF・UI 合格。`dev_plugins\DupliX.aex` は最新。
- **次の 1 手（利用者に相談）:** 残り（3271 枚）は AE の精密な対の判定 `sub_18002B350` 約 160 ms（並列にしても速くならない）、AE がコピーごとに行う描画の準備 約 200 ms、GPU 約 90 ms、計画 約 50 ms。大きく縮めるには `sub_18002B350` や描画の準備を DupliX の計算に置き換える必要があり、見た目が変わる危険がある（突き合わせで確かめながらなら可能）。**回答待ち。**

## 再開の手順（2026-11-26 18:20、利用枠の見張り 70% で中断。利用者「順に進めてください」＝ 1 GPU → 2 計画 → 3 AE の並べ替え）

- **済み（この節の commit、試験は一部だけ）:**
  1. **GPU のまとめ描き:** 段ごとの起動（851 段）をやめ、画面の行ごとに 1 ブロックで、その行のコピーの区間を AE の順に描く 1 回の起動にした（`AeSpanRowsOrdered`、`ae_span.cu`。CPU 側は y の数え上げの並べ替え、`gpu_raster.cpp`）。行の中の分担を細かくした（`cMin`）。Test C 3271 枚で GPU 250〜270 → 約 135 ms。**確かめた:** GPU ALL PASS、Test C 3271・380 枚のオン＝オフ差 0（`cMin` の変更前の版で）。
  2. **計画:** 手前・奥の計画が打ち切りになった comp は次のフレームから組だけの計画へ直行（16 フレームごとに試し直し、`merge_cache.cpp` の `s_over`）。組だけの計画は 3D の交わりの判定の前に奥行きの範囲で外す・外接の四角を詰めた配列で絞る。Test C 3271 枚で計画 115 → 約 53 ms。**確かめた:** 単体 80 合格。
- **未確認（再開したら最初に）:** `cMin` と計画の変更のあとの AE の試験一式（`run_merge.ps1 -Mfrs OFF,ON -CompareDraft -GpuDraw -ForceD "200,150,300"`、EDIT、`run_user_merge.ps1` の Test A・B・C・Test、画面の経路の Test C のオン＝オフ）。
- **次の 1 手（3 の調査の途中）:** AE の並べ替え（Test C で約 250 ms）の中身を測る。`InstallPairTestProf`（`bee_hooks.cpp`、+0x2ACEA の対の判定の呼び出しを中継して数と時間を数える）は書いてビルドだけ通した。**まだどこからも呼んでいない**: `InstallGraphHook` の後で `DUPLIX_GRAPH_PROF=1` のときに呼び、`HookGraph` の後などで `LogPairTests()` を出す。結果で決める: (a) 全組み合わせの数え上げが重い → `sub_18002AB40`（+0x2AAD1 から呼ばれる、辺を作る O(n²) の部分）を、外接の四角が重なる対だけ AE の対の判定 `sub_18002A630` を呼ぶ形に置き換える（隣の一覧は相手の一覧の順の昇順になるので、その順に作れば AE と同じ。節の構造は 0x30 バイト: next, prev, +16 面, +24 隣の一覧の頭, +32 数, +40 旗）。(b) 精密な判定 `sub_18002B350` が重い → 置き換えは見た目の危険が大きいので利用者に相談。
- 計画の内訳の記録（`group time: plan ... candidates, overlapping`）は `DUPLIX_A3D_TIME=2`。

## 再開の手順（2026-11-26 18:00、高速モードとの比較を計測・利用者の判断待ち）

- **今どこまで（この節の commit で push 済み）:** 利用者「はい（まず計測）。Test C（`C:\Users\yakuruto\Desktop\DupliX Test C.aep`、写しは `tests/ae/user/`、3271 枚、家は非表示）も使って。高速モードは 60 ms 前後」。計測した結果、**span が組の描画を含んでいなかった**ことが分かり直した（記録 8d 節「計測の誤り」と表）。直した数字: Test C 3271 枚 高速 53 / 軽量化オン 634 / オフ 約 4000 ms、Test A 380 枚 6 / 369 / 690、Test B 380 枚 18 / 287 / 547。昼までに伝えた軽量化オンの数字は少なすぎた（利用者に訂正が必要）。
- **内訳（Test C 3271 枚、組 1 つ）:** 計画 約 110、AE の並べ替え 約 250、描画 約 470（GPU のまとめ描き 約 300）。
- **次の 1 手:** 利用者に訂正と内訳を報告し、どこまで進めるかを確かめる（候補: GPU のまとめ描きの無駄、計画の速さ、AE の並べ替えの置き換え、行の範囲の計算の置き換え）。**回答待ち。**
- 計測: `$env:DUPLIX_PV_COUNTS='3271'; pwsh tests/ae/run_preview.ps1 -Aep "tests/ae/user/DupliX Test C.aep" -Layer "flower_blue.png" -Frames 6 -IntervalMs 3000 -Variants "pv_fast3271,pv_on3271,pv_off3271"`。内訳は `DUPLIX_A3D_TIME=2`（`=1` は補助のフックも入って重い）。

## 再開の手順（2026-11-26 17:15、Test B の「絵が消える」を修正・利用者の確認待ち）

- **今どこまで（この節の commit で push 済み）:** 利用者「回転で傾けると大量の絵がランダムに消えます」（`C:\Users\yakuruto\Desktop\DupliX Test B.aep`、写しは `tests/ae/user/DupliX Test B.aep`。380 枚、回転の範囲 70、散らばりが画面より広い）を再現して直した。原因: 組の代表に画面の外のコピー（カメラの近くで大きい）が選ばれ、AE がその面を捨てたので組の全部が消えた。8c 節の組にも同じ穴。修正: 代表は画面の内側に写る面積が最大で不透明度が 0 でないコピー（`OnScreenArea`、`merge_plan.cpp`）。詳細は記録の 8d 節「不具合」。
- **確かめた試験:** Test B（書き出し 8 フレーム、画面の経路 100 / 380 枚、ビューアーの拡大 4 倍の指定つきも）オン＝オフ差 0、MERGE ALL PASS、EDIT ALL PASS、Test A・Test 差 0、単体 80 合格。画面の経路 Test B 380 枚: オフ 552、オン 82 ms。今の `dev_plugins\DupliX.aex` は最新（16:53）。
- **次の 1 手:** 利用者に報告し、AE を再起動して Test B で確かめてもらう。
- **利用者の質問（17:30）と回答待ち:** 「描画は GPU で 0、高速モードとの差は準備か。準備も DupliX で担当して高速モード並みにできないか」→ 回答: 画素はほぼ 0、残りは AE のコピーごとの準備（1 枚ずつのコピー 約 0.3 ms、組の中 約 0.1 ms）。案: 並べ替えの前に全コピーの面を AE の一覧へ入れる（`HookGraph` の前段で list に足す。並べ替え・交わりの合成は AE のまま、サブレイヤーの処理は全部消える。見込み 380 枚 40〜50 ms、未計測）。危険: 交わる塊の描画（`sub_180013470`）が同じレイヤーの補助を二重に作り止まるおそれ、足した面の後片付けと進み具合の数。提案中: まず同じシーンで高速モードと軽量化を画面の経路で測ってから決める。**回答待ち。**
- **気を付けること:** ビューアーの拡大（`DUPLIX_PV_ZOOM`、`preview_step.jsx`）は、取り込んだ絵が 1920×1080 のままで、本当に一部だけを描いたかは確かめられていない。拡大して消えるなら、AE の視野がコンポの一部になって代表が捨てられている。その場合は代表が一覧にないときに組を描く仕組み（`HookGraph` が「a representative culled」をログに出す）が要る。

## 再開の手順（2026-11-26 16:10、手前・奥の組を実装・既定で有効・利用者への報告と試用待ち）

- **今どこまで（この節の commit で push 済み）:** 利用者「次の段階の手前、奥のまとめを実装して」に対して実装し、既定で有効にした（`DUPLIX_GROUP_FB=0` で止める）。詳細は `docs/experiments/2026-11-26-dev-log.md` の 8d 節。要点: 組分け `BuildFrontBackGroupPlan`（`merge_plan.cpp`。重なる組の外の面すべてよりはっきり手前／奥のコピーを、レイヤーごとに手前の組・奥の組に）。描く順は `HookGraph`（`bee_hooks.cpp`。Advanced3D +0x14630 の並べ替えの呼び出しを中継に差し替え、手前の組の代表の成分を末尾、奥の組の代表を先頭へ）。機械語が合わなければ取り付けず、計画も 8c 節の組だけに戻る。
- **確かめた試験（既定の設定）:** MERGE ALL PASS（104 本で手前・奥の組を描いた、「not moved」0 件）、EDIT ALL PASS、利用者のシーン（Test A・Test）オン＝オフ差 0、画面の経路 20・100・380 枚 差 0、REF・UI・GPU・単体 78 合格。今の `dev_plugins\DupliX.aex` は最新（Release、15:54）。
- **結果（画面、Test A、span）:** 20 / 100 / 380 枚 = 16 / 23〜26 / 119〜126 ms（組だけ 14 / 23 / 144）。計画を速くした後に MERGE・EDIT・利用者のシーン（8 フレームとも差 0）・単体 79 を回し直して合格。密集では 380 枚中 35 枚ほどしかまとまらず、大きくは速くならない。
- **次の 1 手:** 利用者に結果と限界を報告し、AE を再起動して試してもらう。続けるなら候補は (1) 組を AE の順の途中に差し込む（手前と奥の両方に何かがあるコピー＝380 枚で約 165 枚）、(2) 余白を縮小率に合わせる（6 px 以内に近いコピー、等倍 1 px で組 35 → 100 枚）。
- **気を付けること:** AE の対の判定（`sub_18002B350`）は多角形が少しでも交われば辺を作る（しきい値 1e-12）。辺がないのは重ならない対だけ。組分けはこれに頼っている。中継のメモリは Advanced3D の近くに `VirtualAlloc` で取る。計画は 380 枚で 6〜9 ms、40 ms を超えたら組だけの計画に切り替える。1/4・1/8 の縮小表示は試していない（余白 3 px）。

## 再開の手順（2026-11-26 14:50、組の描画を実装・既定で有効・利用者の試用待ち。セッションをクリアする前に記録）

- **今どこまで（commit b5b0912 まで push 済み）:** 利用者「組の方式に取り掛かってください」に対して、**組の描画を実装し既定で有効にした**（`DUPLIX_GROUP=0` で止める）。詳細はすべて `docs/experiments/2026-11-26-dev-log.md` の 8c 節（仕組み・AE の関数の RVA・ずれの原因 2 つ・結果・注意）。要点: 他と重ならないコピーをサブレイヤー 1 枚（代表＝画面上で一番大きいコピー）にまとめ、AE が代表を描く時に、組の全コピーの面を AE 自身の関数で作って描く（`DrawGroup`、`bee_hooks.cpp`）。組分けは `BuildIsolatedGroupPlan`（`merge_plan.cpp`）。GPU はためてまとめて 1 回（`FlushGroupJobs` → `GpuAeSpansBatch`、詰めた行の区間・重なりの段ごとに 1 回の命令）。奥行きの補正の番号は `HookGetLayerIndex` で組なしの値に戻す。
- **利用者に報告済み（14:45）:** 結果の表（Test.aep 100 枚 38.6 → 22.2 ms、Test A 100 枚 約 30 → 23 ms、Test A 380 枚は密集で 2〜3 枚しかまとまらず変わらない）、オフと全部差 0、限界（密集では効かない・画素の計算は減らない・穴）。**回答待ち:** AE を再起動して試してもらう。続けるなら次は「手前・奥の組」（家と重なっても、はっきり手前か奥にあるコピーをまとめる。8c 節の最後）。
- **確かめた試験（組あり）:** `run_preview.ps1 ... -Edit 5` EDIT ALL PASS、`run_merge.ps1 -Mfrs OFF,ON -CompareDraft -GpuDraw -ForceD "200,150,300"` MERGE ALL PASS（234 本中 72 本の記録で組を描いた、16/32 bit・MFR オンを含む）、`run_user_merge.ps1`（Test A・Test）オン＝オフ、`run_ref`・`run_ui`・単体（74）・`gpu_test` 合格。画面の経路のオン＝オフは 20・100・380 枚と色が変わる変種で差 0。今の `dev_plugins\DupliX.aex` は最新（Release、14:38）。
- **計測の方法（利用者は画面で測る）:** `pwsh tests/ae/run_preview.ps1 -Aep "tests/ae/user/DupliX Test A.aep" -Layer "flower_blue.png" -Frames 8 -IntervalMs 1500 -Variants "pv_off100,pv_on100"`（span の平均。ぶれ ±10 ms）。オンとオフの一致は `spike/.venv/Scripts/python.exe tests/ae/cmp_frames.py build/test/preview pv_off100 pv_on100`。変種は `preview_setup.jsx`（`pv_on1/20/100/380`・`pv_off…`・`pv_none`・`pv_anim_on100/off100`）。Test.aep には `-Layer` 不要。
- **気を付けること:**
  - 穴: 代表の描画で組の描画が断られる（描き込み先の種類が 1 以外、動きのぼかし）と組の他のコピーが描かれない。ログ「group: not drawn as a group」。組分けの条件では起きないはずで、試験でも出ていない。
  - 1 回の描画の終わりの検出（`ctx+252 >= ctx+248`）は画面の経路で成り立たないことがある（「pass time」の draw の数字は当てにならない）。共有の表は次の `AEGP_GetNumLayersToRender` で必ず捨てているので結果には影響しない。
  - 簡易プロファイラー（`DUPLIX_A3D_PROF`）は GPU の同期待ちを大きく見せる。描画の費用は `DUPLIX_A3D_DRAW=2` や `DUPLIX_GROUP_NOGPU=1` との差で測る。
  - 診断の環境変数（記録の 6・8c 節）: `DUPLIX_A3D_TIME`、`DUPLIX_GROUP_TRACE`（段階ごとの記録と AE の内部確認の失敗）、`DUPLIX_XF_CHECK`、`DUPLIX_GROUP_CLONE`、`DUPLIX_GROUP_BATCH=0`、`DUPLIX_BOUNDS_SHARE=0`（共有せず比べる）。
  - AE の内部の関数をアドレスで呼んでいる（Advanced3D 2025 の RVA）。版が変わると使えない（vtable の照合で描画のフックごと止まる）。
  - 計測のスクリプトが AE に届かない件は直した（AE の「2 つめのスクリプト」の警告。`run_preview.ps1` が待ち・送り直し・警告を閉じる）。

## 再開の手順（2026-11-26 12 時、組の方式を検討中・利用者への報告前）

- 利用者の案「他の絵と交わらないコピーを一枚絵にまとめる（手前・奥の組）」に「進めてください」、「以前の実装を流用できるのでは」。回答済み: できる（完全一致のまま）、流用できる部分（`merge_plan`・`ae_order`・Count/Rects の群れの面・GPU の土台）と、新しく要る部分（コピーごとの描画の行列と行の範囲を AE と完全に同じに作る）。
- **済み（この節の commit）:** ストリームの値の共有（`HookLayerStreamValue`）。画面の経路で 100 枚 29〜33 ms、380 枚 85〜101 ms。試験一式合格（記録の 8b 節）。計測の道具の修正（AE の「2 つめのスクリプト」の警告で止まっていた）。
- **分かったこと:** 画素を描く費用は今は誤差（DRAW=2 と同じ）。残りは AE のサブレイヤーごとの準備（1 枚 約 0.2 ms）。組の方式はこれを消せる（見込み: 380 枚 約 20 ms 前後、1 枚の変種が約 10 ms）。
- **次の 1 手:** 利用者に今の結果と組の方式の費用（AE の行列の計算の再現＝数時間〜、検証は今の 1 枚ずつの描画で AE の行列と比べる）を報告して進め方を確かめる。手早い候補: 不透明度の問い合わせの共有。

## 再開の手順（2026-11-26 11 時、残りの軽量化を実施・利用者への報告待ち）

- 利用者は 8 時の版を画面で確かめ「計測結果と同じ。信用できる。残りの軽量化も進めて」。
- **済み（この節の commit）: (1) レイヤーの範囲の共有（`HookGetBounds`。Advanced3D がサブレイヤーごとに問う `AEGP_GetRenderLayerBounds` に、同じ描画の準備の中では 1 回目の答えを返す。全 792 回で答えが同じことを確認）、(2) GPU への絵の再転送を省く（控えと各行の画素が同じなら送らない）、(3) 小さいコピーを AE に任せるしきい値 `DUPLIX_GPU_MINPX`（既定 0＝常に GPU。差がなかったため）。** 詳細は `docs/experiments/2026-11-26-dev-log.md` の 8 節。
- **結果（画面の経路、Test A、span）: オン 20 / 100 / 380 枚 = 14 / 36 / 151 ms（8 時の版 19 / 57 / 237）。オフ 68 / 195 / 619。目標「100 枚で 50 ms 以下」を満たした。** ぶれ ±10 ms。
- **確かめた試験:** EDIT ALL PASS（再実行で。1 回目は AE の起動の不調とみられる「no frames」）、画面のオン＝オフ 4 種類差 0、MERGE ALL PASS、利用者のシーン オン＝オフ、REF・UI・単体・GPU 合格。今の `dev_plugins\DupliX.aex` は最新（Release）。
- **次:** 利用者に報告して試してもらう（AE の再起動が必要）。残りの候補はフックの時間（380 枚で約 20 ms）と GPU の往復をまとめること（記録の 8 節）。`run_preview.ps1` がときどき結果なしで終わる件は、次に起きたら原因を追えるよう表示を足す。

## 再開の手順（2026-11-26 8 時、画面での速さを修正済み・利用者の試用待ち）

- **済み（commit 2bbac0d、push 済み）: 利用者の報告「新しい軽量化でも画面のレンダリング時間が変わらない（380 枚 700 ms〜1 秒）」を再現して直した。** 詳細は `docs/experiments/2026-11-26-dev-log.md`。原因は画面の経路でだけコピーごとに起きていた 2 つ: (1) AE がコピーごとにテクスチャを取り直す（約 1 ms）→ 軽量化オンのレイヤーは 1 フレームの描画の中で 1 枚目の複製を返す（`HookTex` の共有）。(2) サブレイヤーごとの PreRender が毎回フックのキャッシュの世代を進め、不透明度のフックが毎回 AEGP で読み直していた（約 0.55 ms）→ パラメーターとアンカーの指紋が変わったときだけ進める（`ParamsChangedAt`）。ほかに、テクスチャのフックの以前の方式の処理（約 0.9 ms）を `DUPLIX_MERGE_OLD=1` のときだけにした。
- **結果（画面の経路 `run_preview.ps1`、Test A、span、最終ビルド 8:29）: オン 20 / 100 / 380 枚 = 19 / 57 / 237 ms（修正前 84 / 323 / 1008）。オフは 77 / 228 / 689。オンとオフの絵は差 0。目標の「100 枚で 50 ms 以下」にはまだ届いていない。** レンダーキュー: オン 152、オフ 183、花なし 163 ms/フレーム。
- **テクスチャの共有の確認（助言役の指摘）:** 花の色が毎フレーム変わる変種 `pv_anim_on100`/`pv_anim_off100`（DupliX の前に塗り＋時刻の式）で、オン＝オフ差 0、連続フレームは実際に変わる。編集の試験に「塗りの式を同じ時刻で変える」手順を追加し、EDIT ALL PASS。
- **確かめた試験:** 新しい編集の試験 `run_preview.ps1 ... -Variants "pv_anim_on100,pv_on20" -Edit 5` EDIT ALL PASS、`run_merge.ps1 -Mfrs OFF,ON -CompareDraft -GpuDraw -ForceD "200,150,300"` MERGE ALL PASS（共有が働いたことをログで確認）、`run_ref` REF ALL PASS、`run_ui` UI ALL PASS、単体 ALL PASS、`gpu_test` GPU ALL PASS、`run_user_merge.ps1`（Test A・Test）オン＝オフ。
- **ビルドの既定を Release にした**（`tools/build.ps1`。それまで利用者が試していたのは Debug 版）。今の `dev_plugins\DupliX.aex` は Release（8:29 のビルド＝最新の commit）。
- **利用者の回答待ち:** 新しい版を画面で試してもらう（20・100・380 枚、オン・オフ。AE の再起動が必要）。目標は「100 枚で 50 ms 以下」（今は span 57 ms）。
- **次の候補（利用者の結果しだい）:** (a) PreRender がコピーごとに約 1 回来る（呼び出し元は AE の描画計画 BEE/RG と、Advanced3D+0x8b72 の各サブレイヤーの範囲の問い合わせ＝canvas suite の +128 付近。`DUPLIX_PRE_STACK=1` で記録）。そのたびに編集検出の指紋（約 30 個の `PF_CHECKOUT_PARAM`＋アンカー）で 1 回 約 0.15 ms＝100 枚で約 15 ms。指紋の安い方法か、範囲の問い合わせの共有を検討。(b) フックの時間（380 枚で約 75 ms）、(c) GPU のテクスチャの転送が共有のたびに起きる（`memcmp` で省けば 100 枚で約 10 ms 減の見込み）、(d) GPU の往復を連続するコピーでまとめる、(e) 前からの候補（16/32 bit・画質 2、群れの方式のコードの整理、手引きの「軽量化」の節の書き直し）。
- 気を付けること: 診断の道具は環境変数（`DUPLIX_A3D_TIME`・`DUPLIX_A3D_PROF`・`DUPLIX_A3D_AEPIX`・`DUPLIX_CMDLOG`、記録の 6 節）。簡易プロファイラーは相手のスレッドを止めたままスタックをたどると AE が固まる（直した）。判定の Python は `spike/.venv`。

## 再開の手順（2026-11-26 朝、新しい軽量化を実装済み・利用者の試用待ち）

- **新しい軽量化（commit e90c6cb、push 済み）: 「軽量化」がオンのレイヤーのコピーは、AE の描画（面の順番・準備・行の範囲）はそのままに、画素の計算だけを DupliX が GPU で AE と同じ計算で行う。** 解読の全記録は `docs/experiments/2026-11-26-a3d-span-notes.md`。計算の本体 `src/core/ae_span_impl.h`（CPU と CUDA で共通）、CPU 版 `src/core/ae_span.{h,cpp}`、GPU `src/gpu/ae_span.cu`（`--fmad=false`、`tools/build_ptx.ps1` が `ae_span_ptx.h` を生成）と `GpuAeSpans`（`gpu_raster.cpp`、コピー 1 枚ずつ、写る範囲だけ転送、テクスチャは `g_texGen`（HookTex で増える）で再転送）、フック `bee_hooks.cpp` の `HookA3dDraw` → `DrawCopyOurs` → `OurIterate`（対応外: 描き込み先の種類が 1 以外＝循環の塊など、8 bit 以外、動きのぼかしの標本 2 以上、画質 2、合成 2 以外、ライト、マスク、合成の表、CPU の旗 0、GPU の失敗 → AE が描く）。チェックの値はコピーの不透明度のフックで読んだ値（`NoteMergeSeen`、全スレッド共通）。以前の群れの方式は `DUPLIX_MERGE_OLD=1` のときだけ。作りかけの順序グラフの規則（0225fd9）は戻した（`ae_order.*` は使われないまま残っている）。診断 `DUPLIX_A3D_DRAW`: 0 AE だけ、1 記録、2 コピーを飛ばす、3 捕獲（`DUPLIX_A3D_CAPSUB`=番号の並び、`DUPLIX_A3D_CAPW`=幅、`DUPLIX_A3D_CAPDIR`）、4 画素の計算なし、5 我々の CPU、6 我々の GPU。
- **確かめた試験:** 単体 ALL PASS（74、捕獲との照合は `DUPLIX_SPAN_CAP=<dir;dir>` のとき）、`build/gpu/gpu_test.exe` GPU ALL PASS（捕獲 14 枚＝`build/test/cap1`・`capT/s*`・`capA/s*` を GPU で描いて AE とビット一致）、`run_merge.ps1 -Mfrs OFF,ON -CompareDraft -GpuDraw -ForceD "200,150,300"` MERGE ALL PASS（オンとオフが全部の回で完全一致、`check_merge.py` を新方式の判定に変更）、`run_ref.ps1` REF ALL PASS、`run_ui.ps1` UI ALL PASS。利用者のシーン（`run_user_time.ps1`、`DUPLIX_A3D_DRAW=6` で）: Test.aep 100 枚 30 フレーム完全一致・花の分 157→16 ms、Test A 30 フレーム完全一致・花の分 43→20 ms。
- **未解決: 利用者の「レイヤーのレンダリング時間」（画面）は Test A の 380 枚で約 900 ms だが、レンダーキューでは花の分が 15〜30 ms しかなく（ぶれ ±20 ms）、利用者の測り方を再現できていない。** 画面では別の重さ（コピーごとの何か）がある可能性。→ 利用者に新しい版で同じ表示を見てもらう（20・100・380 枚、オン・オフ）。重いままなら `run_preview.ps1`（画面の経路）で内訳を調べる。
- 次の候補: 画面の重さの調査、16/32 bit・画質 2（バイキュービック）への対応、群れの方式のコード（merge_plan など）の整理、手引き `docs/duplix-quickstart.md` の「軽量化」の節の書き直し。

## 再開の手順（2026-11-26 午後、規則の方針を利用者に相談中）

- **単体テストの 9 件の FAIL の原因（診断の表示を `unit_tests.cpp` に追加して確認）:** (b) `merge_tilted_wall_sides` は手前の 5 枚が同じ z=100 のカメラ正面で重なる → ties 8（循環 0）。(c) `merge_ambiguous_no_merge` は回した 1 枚が壁と画面で重ならない（y≥202、壁は 200 まで）ので AE の規則では辺がなく循環 0（ties 7 は同じ z の 5 枚）。試験の配置が家の例を再現していない（`aegraph.py` の家の連鎖のような、橋渡しの面がいる）。field 1000 枚は循環 9 枚、free 5000 枚は全部が 1 つの塊（回したコピー同士が本当に交わる）。stress は時間切れ。
- **AE の並べ方の追加の解析（`spike/ida/dump_a3d_all/`）:** `sub_18002AA00` が面の一覧の順に DFS の根を選ぶ。辺は組の列挙順（一覧の順）に足す。`sub_18002A880` は Tarjan の変形（番号ではなく DFS の深さで low を持つ）、`sub_18002A7D0` が塊を集め、塊は結果の一覧の先頭に足す。差のない組（ties）には辺がないので、その順は DFS のたどり方で決まり、ほかの面の有無で変わりうる（例: 一覧 [Z, X, Y]、Z→Y だけあると Y が X より先に出る）。面の一覧（`sub_180014470` の 232 バイトの配列）の順番は `sub_18000FD40`/`sub_180015500`/`sub_180013160` で作られ、まだ読んでいない。
- **前の試験の事実:** 旧規則の群れ分けは、交わるコピー（`OrderItems` が 0）を 1 枚ずつにしていた。`wall_front_back`（回転あり 30 枚）などは合格 → コピーだけの塊を 1 枚ずつにする方式は、少なくともその場面では一致した。家の例は、ほかのレイヤー（家）を含む塊からコピーを抜いたのが原因。
- **利用者に相談中（返事待ち）:** (1) 今の厳密な規則（循環か ties が 1 つでもあればフレーム全体をまとめない。平らな散らばりと密集した回転では効かない）、(2) 緩めた規則（ほかのレイヤーを含む塊・ties があればフレームをまとめない。コピーだけの塊・ties のコピーは 1 枚ずつにして残りをまとめる。まとめた後のグラフで塊が同じで群れの面が塊に入らないことを確かめる。AE の試験に密集・平らなケースを足して確かめる）、(3) AE のたどり方を一覧の順まで完全に再現する（時間がかかる）。おすすめは (2) → AE の試験 → ずれたら (3)。
- 速さの修正（`allNb` の組を使い回す・予算で打ち切る）は方針が決まってから。
- **（11/26 未明、利用者「最後まで進めてください」）AE のコピー 1 枚の描き方を解読中。全部 `docs/experiments/2026-11-26-a3d-span-notes.md` に記録（呼び出しの流れ・データの形・各関数の訳・捕獲の使い方）。** 捕獲（`DUPLIX_A3D_DRAW=3`）は動く（`build/test/cap1`、Test.aep のコピー 0、786 行）。PF.dll は `spike/ida/versions/2025/PF.dll(.i64)` に解析済み。**次: (1) PF の定数と `qword_180425A18`・範囲平均 0x18008A560 の機械語を読む → (2) CPU の参照版 `src/core/ae_span.{h,cpp}`（AE に依存しない）と、捕獲と比べる試験（単体テスト or `tests/tools` の道具）→ ビット一致 → (3) GPU 版 → (4) `HookA3dDraw` の本番（kind 1・標本 1・8 bit・画質 0/1・合成 2・ライトなし・マスクなしだけ置き換え、他は AE）→ (5) 利用者のシーンで実測。**
- **（11/26 深夜 3）利用者の計測（レイヤーごとのレンダリング時間、Test A、軽量化オン）: flower_blue 380 枚 約 900 ms、20 枚 約 50 ms、100 枚 約 250 ms（1 枚 約 2.5 ms）、家 1〜2 ms。目標「100 枚で 50 ms 以下」。** 利用者には新方式の見込み「100 枚 10〜15 ms、380 枚 30〜40 ms」を伝えた。**確認 1（助言役の指摘、見込みの前提）:** `run_user_time.ps1 -Aep "tests/ae/user/DupliX Test.aep" -Count 100 -Variants off,none`（`-Count` を追加）を 2 回: 花なし 141〜159、通常 246〜249、`DUPLIX_A3D_DRAW=2`（飛ばす）153〜158 ms → 飛ばした後の残りは 100 枚で 6〜17 ms（1 枚 約 0.1 ms）。前の「1 枚 1.3 ms 残る」は `run_user.ps1 -SkipScene` が Test A の版を使っていた見かけ。**確認 2（完全一致の可否）:** Advanced3D 全体に `rcp`/`rsqrt`/FMA は 0 件（スクラッチパッドの `scan_approx.py`。8 bit の画素の処理と標本化の関数 24 個の機械語も書き出した）。単精度の四則・`vroundsd`・`vcvttss2si` なので、GPU で同じ順の計算（`--fmad=false`、IEEE の除算・平方根）なら一致できる見込み。`PF_Get_Xfer` の合成の関数は別の DLL（未確認）。**次（助言役の形）:** (1) 捕獲: 反復のラッパーの `d`（多角形の経路の v36: +8 が描画の状態 v45＝+16 テクスチャの PF_World、+24 SAT、+32 標本の項目（72 バイト）、+40 その数）で、選んだコピーの各行の範囲 [x0,x1) を元の行の関数の前後で写し、状態と絵も書き出す → (2) 機械語から命令ごとに訳した CPU の参照版を捕獲とビット単位で一致させる → (3) GPU 版を CPU の参照版と一致させる。本番の条件: `*(int*)a7 == 1`（直接の描き込み）かつ動きのぼかしの標本 1 つのときだけ置き換え、種類 2〜4（断片の表＝循環の塊を含む）は AE のまま。転送は連続するコピーの範囲だけ（全画面の往復は 約 12 ms）。
- **（11/26 深夜 2）追加の計測（実験コードに絵の受け取りの時間 `a3d tex time` と、行ごとの処理の CPU 時間 `row-work` を追加）:** (1) 小さな花 1 枚の描画 約 1.4〜2 ms のうち、行の処理の CPU 時間の合計は 約 1.5 ms＝本当に画素の計算が重い（並列の立ち上げの手間ではない）→ AE と同じ計算の GPU 版が必要という結論は変わらない。(2) レイヤーの絵の受け取りはフレームに 1 回（サブレイヤーごとではない、2 ms 前後）。(3) Test A（レンダーキュー、MFR オフ、30 フレーム。PC の負荷で ±20 ms ぶれる）: 花なし 140〜166、通常 217、コピーを飛ばす 168 ms → 花の分は 約 50 ms、飛ばすと 約 28 ms（380 枚の AE 側の準備、1 枚 約 0.07 ms）。**注意: `run_user.ps1 -SkipScene` は前回作った版を使う。途中で Test A で版を作り直したので、それ以降の `v_norm100` は Test A のもの（380 枚）だった。** 利用者に「花を隠したときのレンダリング時間」を質問中（家だけの基準を知るため）。
- **（11/26 深夜）利用者の測り方: コンポジションパネルのレンダリング時間、フル画質、Test A を開いた状態（家 1 つ、表示は青い花だけ約 20 枚、花の描画に約 130 ms）。** デスクトップの `DupliX Test A.aep` は `tests/ae/user/` のものと同じ内容（md5 一致）だったのでデスクトップ側を消した。新しい計測 `pwsh tests/ae/run_user_time.ps1 -Aep "tests/ae/user/DupliX Test A.aep" [-Frames 30] [-Variants asis,on,off,none]`（`user_time.jsx`: コピーを開き、MFR オフで種類ごとに連続フレームをレンダーキューで描いて ms/フレーム）。結果（30 フレーム）: そのまま 208、軽量化オン 205、オフ 203、花なし 151 ms（レンダーキューの TIFF 書き出しなどを含む。差の約 55 ms が花）。青い花は 380 枚（画面に入るのは一部）。`DUPLIX_A3D_DRAW=1` の記録では、オフの 10 フレームで AE がコピーを 1920 幅の画面に 369 回描いて 1071 ms（約 107 ms/フレーム、1 枚 約 2.9 ms）。**注意: 隠れた AE は開いたプロジェクトのビューアーも裏で描く（512 幅のバッファに元のコンポの花を 368 回）ので、記録は描き込み先の幅で分ける。家の描画は 10 フレームで 1 回しか記録されない（別の経路？未確認）。** 家があるので軽量化（群れ）は効かない。**方針: 新方式（AE の順のまま、コピーの画素の処理を DupliX が GPU で AE と同じ計算で行う）を進める。次: 8 bit の画素の処理 `sub_180037D90`/`sub_180045210` の読解（どちらが使われるか＝a1+40 の意味、標本化の方式、合成）。**
- **（11/26 夜）利用者「100 枚で 200 ms は遅すぎ。100 ms 以下に。80 ms も嘘では？ Test A で 20 枚ほどでも 130 ms 前後」。** 測り直し（`DupliX Test.aep`、aerender MFR オフ 30 フレーム、秒単位の合計 ÷ 30）: コピー 0 枚 67 ms、軽量化 10 枚 133 ms、100 枚 167 ms、通常 100 枚 1870 ms → **前に報告した 83 ms は再現しない（誤り。利用者に謝罪済み）。** 軽量化 100 枚の内訳（`DUPLIX_A3D_DRAW=1`）: 群れの面 1 枚の描画 35〜55 ms（GPU の群れの絵 約 15 ms を含むとみられる）、フレームの間隔 約 150〜165 ms。残り約 50 ms は未特定。Test A（家・4 レイヤー 452 枚）は `user_scene.jsx` の版の作り方が合わず測れなかった。**家があるので、今のビルド（厳しい規則）では Test A は全フレームで軽量化が効かない**＝利用者の 130 ms は軽量化なしの値の可能性。**次: 利用者に測り方（どこで読んだ時間か・枚数・軽量化のオンオフ）を確認 → 同じ測り方で再現 → 内訳（コピーごとの絵の受け取り・面の準備・計画・群れの描画）を測る → 100 枚で 100 ms 以下の方法を決める（候補: 新方式でコピーの絵の受け取りを 1×1 の絵に差し替えて AE の準備を削る）。**
- **（11/26 夕、実験の結果。利用者が案を承認「進めてください」）** 実験コード: `bee_hooks.cpp` の `HookA3dDraw`／`InstallA3dDrawHook`（`DUPLIX_A3D_DRAW`=1 記録、=2 コピーを描かずに飛ばす。SY_TRLayer の vtable は RVA 0x80438、slot 1 = 0x12B30 を照合してから差し替え）。分かったこと: (1) 面（SY_TRLayer、232 バイト）の +224 がレンダー内のレイヤー番号。`PR_ExtractRenderRefcon`（PR.dll）→`BEE_GetArtisanNthLayerToRender` でトレース → 既存の `kTraceArtLayer` でレイヤーとサブレイヤー番号が分かる（AE の試験で全コピーを特定、順番も取れた）。(2) 飛ばすと壁だけの絵になり落ちない。(3) 利用者のシーン（aerender MFR オフ）: 通常 100 枚 約 1900 ms/フレーム → 飛ばす 約 200 ms（コピー 0 枚 67 ms）。(4) 1 枚の描画 14〜17 ms のうち 93% が画素の処理（ctx+224 の並列の反復）、準備は 7%。→ 速くするには AE の画素の処理を DupliX が同じ結果で行う必要がある。(5) 描き込み先は a7 → {int 1, バッファ}、バッファ +32 データ、+40 rowbytes、+44 幅（8 bit で 1920×4）。(6) 画素の処理は `sub_180050B40`（種類 1→`sub_18004CFD0`）→ bpc ごと・a1+40 ごとの版（8 bit: `sub_180037D90` / `sub_180045210`、各 約 950 行、`PF_Get_Xfer`・`PF_LinearBlendingTables`・標本化の関数 `sub_18005C*`/`sub_1800559*` など）。**利用者に報告して返事待ち: AE の画素の処理（まず 8 bit・通常の合成）を GPU で同じ結果に作り直す計画でよいか。対応しない場合（16/32 bit、ライト、モーションブラー、描画モードなど）はそのコピーだけ AE が描く（フレーム全体は無効にならない）。**
- **（追記 2）利用者「条件で軽量化が無効になるフレームがあるのは嫌（重なりが 1 つでもあるとカメラを動かしても常にオフ）。黒魔術でもっと自由にできないか」→ 新しい案を提案して返事待ち: AE の描画の中に入る。** AE には全コピーを 1 枚ずつ渡したまま（並べ方は AE 自身が全部の面で決める＝まとめないときと同じ順）、1 枚の面を描く関数 `sub_180014800`（`sub_180014470` が塊 1 枚ごとに呼ぶ）を差し替え、コピーの面なら DupliX が GPU で描く（AE の順に連続するコピーはまとめて 1 回で）。2 枚以上の塊（`sub_180013470`、断片の表）とコピー以外の面は AE のまま。群れ・判定・面の奥行きの計算が不要になり、無効になるフレームがない。未確認: 232 バイトの面からどのレイヤー・何枚目のコピーかを知る方法、描き込み先のバッファの形式、AE の 1 枚の描き方（補間・縁）との画素の差（今の群れの中身と同じ問題）、ライト・影・被写界深度・動きのぼかしのある面は AE に任せる条件。最初の実験: `sub_180014800` を差し替えてコピーの面の順と中身を記録 → コピーだけ描かずに飛ばす → 自前で描いて AE と比べる。
- **（追記）利用者「難しく考えすぎ。もっと良い方法は？」→ 単純な考え方を提案して返事待ち:** 「答えが 1 つに決まるフレームだけまとめる」。重なる組がすべて前後はっきりで循環がなければ、AE がどの順にたどっても絵は幾何どおりの 1 通り（まとめた後も同じ条件なら一致）。AE の内部の順番の再現は不要。これは今の実装（規則 1）と同じ中身なので、残りは (1) 循環か ties を 1 つ見つけた時点で打ち切る（速さの問題が消える）、(2) 単体テストの期待を「決まるフレームはまとめる・決まらないフレームはまとめない」に直し、家の例は橋渡しの面を入れて作り直す、(3) 実際のシーン（利用者のシーンの家なし、平らな散らばり、密集）でどれだけまとまるかを AE で測る。足りなければ、その時に旧方式（互いに交わるコピーだけ 1 枚ずつ、`wall_front_back` などで一致済み）を足す。

## 再開の手順（2026-11-26 0:50、5 時間枠 70% で区切り。リセット 02:40）

- **今の作業: AE（クラシック 3D）の並べ方を再現して、軽量化の可否を決める（利用者の決定 A）。** 解析の結果と設計は下の「（11/25 23:40〜）」の項。
- **途中の状態（commit は WIP）:** `src/core/ae_order.{h,cpp}`（`AeOrderGraph`: 画面で重なる組の奥行きの差で辺 → Tarjan。循環する面の数と、差のない組（ties）を返す。`CameraFacingSurface`）を追加し、`BuildMergePlan` で (1) 全コピー 1 枚ずつのグラフに循環か ties があればフレームをまとめない、(2) まとめた後のグラフ（群れの面は 24 px 広げる）に循環か ties があればまとめない、に置き換えた（前の「またぎ合う組」の規則は削除。診断 `DUPLIX_MERGE_RULE=1` で検査を外せる）。**単体テストが 9 件 FAIL:** (a) `merge_plan_speed` が遅い（5000 枚 2.6 s、1000 枚 175 ms。全組を多角形で切る O(重なる組) で重い。`allNb` の組を使い回す・境界の箱で先に絞る・予算で打ち切る必要）。field 1000 枚は群れ 0（ties か循環が出ている。原因を確かめる）。(b) `merge_tilted_wall_sides` の突き抜けなしの版でもまとめない（何が循環・ties になるか要確認。板と平行でない面の組で ties が誤って出ている可能性：重なる多角形の頂点が辺の上にあると差 0 になる）。(c) `merge_ambiguous_no_merge` で循環が出ない（家の例を再現したはずの配置。コピーと板が画面で重ならない？）。
- **次の 1 手:** 単体テストの (b)(c) を直す（`AeOrderGraph` の ties の数え方：頂点ごとに差 0 は数えず、どの頂点でも差がないときだけ ties）→ (a) 速さ → `run_user_merge.ps1`（家のシーン 8 フレーム一致、`aegraph.py` と同じ結果か）→ 試験一式（`run_merge.ps1 -Mfrs OFF,ON -CompareDraft -GpuDraw -ForceD "200,150,300"`）。試験一式では `wall_tilted` など突き抜けるケースはまとめなくなるので `check_merge.py` の期待（groups）を直す必要。平らな散らばり（Z 範囲 0）で ties が出ればまとめなくなる点も確認（助言役の指摘）。
- 利用者への報告済み: 解析で分かった仕組み（重なる組の辺 → 強連結成分 → 塊は断片の表）と、新しい規則の方針。予測の確認（家なし＝塊なし、元の不具合＝塊が変わる）は合ったが、「またぐコピーだけ 1 枚ずつ」の計画では塊が同じなのに前後が変わった → 塊の中の描き込み順（グラフ全体のたどり方）に左右されると判断し、「塊が 1 つでもあればフレーム全体をまとめない」にした。

## 再開の手順（2026-11-25 午後、まとめる軽量化の実験中）

- **高速モードの GPU 化は完了**（最終レビューの修正 c6fe738、開発記録 5d019d9、push 済み）。GPU で描けないときは CPU で描き直す、カーネルは GPU の準備で読み込む、帯ごとに起動して中止できる、区画の一覧の上限。後回しにした軽微な指摘は開発記録 `docs/experiments/2026-11-25-dev-log.md` の 5 節の末尾。**利用者にはまだ一覧を伝えていない。**
- **今の作業: 通常モードの軽量化（設計書 12.7 の (3)・12.12、群れを 1 枚の絵にまとめる）。** 利用者が実験に同意（11/25 午後）。利用者と合意した判定の考え方: 群れの外で画面上に重なるもの（他のレイヤー・まとめないコピー・他の群れ）は、群れの中身が写る画素で完全に手前か奥。3D 回転した板は「板のどちら側か」で分け、板を突き抜けるコピーはまとめない。手前の群れの面は群れの一番手前、奥の群れは一番奥の奥行きに置く。重なりはコピー 1 枚ずつの写る形で判定（数画素の余裕）。板の左右の空き場所のコピーは板と同じ奥行きでもまとめてよい。
- **実験は完了**（commit d2b7914。記録は開発記録の 5b 節、差分は `spike/atlas/texture_swap_spike.patch`、実験の道具も `spike/atlas/`）。結論: 並べた絵を M3 で引き伸ばす方式は、まとめないコピーの縁が変わる（最大 3〜7）ので不採用。**AE がコピーごとに絵を受け取る `BEE_RenderTextureForArtisan`（AEGPDriver の IAT）を差し替え、群れの面のコピーだけ自前の絵（`AEGP_WorldSuite3::AEGP_New`。AE が `AEGP_DisposeTexture` で delete する）を渡す方式**が成功（ほかのコピーは差 0、画面と書き出しで動作）。`src/` は元に戻し、`dev_plugins\DupliX.aex` は実験コードなしでビルドし直した（13:23）。
- **設計書は利用者が承認（11/25 午後、「進めてください。後回しにした軽微な指摘も直してください」）。** 軽微な指摘は修正済み（commit 3bc90c5）。
- **計画 `docs/superpowers/plans/2026-11-25-merge-optimization.md` を executing-plans の方式（このセッションで直接）で実行中。** 台帳 `.superpowers/sdd/2026-11-25-merge-optimization/progress.md`（git 管理外。判断 Ruling を全部ここに記録）。Task 1〜6 完了（7cd055e, a036a2d, 308a93c, 116c2a1, 624cd7a。11/25 16:30、5 時間枠 95% で区切り。リセット 17:20）。**次は Task 7（MFR オン・`-Gpu CUDA`・16/32 bpc・画面（run_ui の方式）・300 フレームのメモリ。`run_merge.ps1` に `-Mfrs ON`、`-Gpu`（merge.jsx は `DUPLIX_MERGE_GPU` をまだ使っていない: プロジェクトの GPU 設定と bpc の切り替えを足す）、`-Long` を足す）→ 8（状態の欄に理由・手引き・計測）→ 最終レビュー（executing-plans の手順、台帳の Ruling を利用者に一覧で報告）。**
- **（11/25 17 時）利用者の質問「軽量化がレンダリング時間にあまり効いていない」に回答し、返事待ち**（試した枚数・測り方・レイヤーの条件を質問中。返事で Task 7 の前に改善をするか決まる）。調べた結果（利用者のシーン、aerender MFR オフ、30 フレーム、1 フレームあたり）: コピー 0 枚 67 ms、通常 10 枚 233 ms → まとめる 167 ms、通常 100 枚 1570 ms → まとめる 233 ms。10 枚では群れの面が 1752×1032 とほぼ画面全体になり、AE がその大きな面を描く時間と DupliX が CPU で描く時間が残るので効きが小さい。画面（ビューアー）でも 10 枚・100 枚ともまとめる処理が動くこと（テクスチャの差し替え）を確認。`preview_setup.jsx` に `pv_norm10/pv_merge10/pv_norm100/pv_merge100` を追加。**注意: `DUPLIX_TIMING` の span はフックの呼び出しの範囲だけで、まとめた絵を AE が描く時間を含まない（まとめる版の画面の速さはこの方法では測れない）。** 改善の候補（利用者に提案中）: 群れを画面上の小さな塊に分けて面を小さくする、少ない枚数ではまとめない、群れの絵を GPU で描く。
- （11/25 17:10）内訳を測った（`merge_texture.cpp` に `merge: group draw ... in X ms` のログを追加）: DupliX が群れの絵を CPU で描く時間は 10 枚で約 38 ms、100 枚で約 87 ms。残り（10 枚で約 60 ms）は AE がクラシック 3D でほぼ画面大の面を描く時間で、こちらは CPU のままで変えられない。GPU 化で減るのは DupliX の分だけ（10 枚 167 → 約 130 ms の見込み）と利用者に説明。
- （11/25 17:20）利用者の案「群れを 2D レイヤーとしてレイヤー順に割り込ませる」に回答し、返事待ち。回答の要点: 2D を挟むと AE は 3D レイヤーをそこで別の塊に分けて描く（塊をまたぐ交差・影・前後の判定が消え、レイヤー順と奥行きが食い違うシーンで見た目が変わる）ので完全一致の条件に合わない。2D の速さの元は「画素ごとの補間をしない」ことで、群れの面は画面に 1 対 1 に重なるので、群れのコピーだけ AE にドラフト画質（最近傍）で描かせれば同じ絵のまま補間を省ける見込み（11/24 の計測: 10 枚で最高画質 161 ms → ドラフト 55 ms）。この実験（群れのコピーの画質の問い合わせをフックでドラフトにする→速さと画素の一致を測る）をしてよいか質問中。
- **（11/25 18:30）ドラフトの実験（利用者が承認「進めてください」）。** 実装: `bee_hooks.cpp` の `HookGetLayerQuality`。Advanced3D.aex は `AEGP Layer Suite` を版 10 で取り、`AEGP_GetLayerQuality`（表の +64）で画質を読む → その版の表の 1 か所を差し替え、呼び出し元が Advanced3D で、まとめるオンで、そのフレームの計画のサブレイヤーが全部群れの面のときだけ DRAFT を返す。フレームの時刻は同じスレッドの Count のフックの時刻（不透明度より先に画質を聞かれるため。テクスチャのときの時刻との食い違い 0 件）。**今は環境変数 `DUPLIX_MERGE_DRAFT=1` のときだけ有効（実験）。** 結果: 画面（PNG 保存込み）10 枚 327→288 ms、100 枚 383→340 ms、絵は差 0。aerender（「最良設定」でも効く）10 枚 133→108 ms、100 枚 192→167 ms（コピー 0 枚は 50 ms）。`run_merge.ps1` 一式（MFR オフ）でフックの有無の絵が 46 枚とも完全一致。`seam` はどちらも FAIL だが `-ForceD` なしで流したため（既知、変更とは無関係）。MFR オン・フックありも、フックなし（MFR オフ）の絵と 23 枚とも完全一致（seam は同じく `-ForceD` なしの既知の FAIL）。利用者の決定（「おすすめの進行で」）: (1) ドラフトを正式に入れる → (2) 群れの絵の GPU 化。
- **（11/25 21 時）(1) は実装済み: ドラフトは常に有効（`DUPLIX_MERGE_DRAFT=0` で切れる。毎回読む）。** 試験 `run_merge.ps1` を利用者の指摘で作り替えた: 既定は 1 つの AE の中で `merge.jsx` がシーンを作って全コンポを書き出す（`-Mfrs OFF,ON -CompareDraft -ForceD "200,150,300"` で 87 件 160 秒。前の方式は `-Aerender`）。プラグインはログの出力先・キャッシュのソルト・ドラフトの切り替えを毎回読み、ログの回数制限（`LogLimit`）と計画のキャッシュはログの切り替えで数え直す。結果: 前からの試験は MFR オフ・オンとも全部合格（seam も）。ドラフトの有無で 27 件一致。
- **見つかった前からの不具合（ドラフトと無関係、利用者に報告して判断待ち）:** 新しい試験 `group_res5`（1/5）と `group_odd_res3`（401×301 の 1/3）が FAIL。(a) テクスチャの縮小率を「テクスチャの大きさ ÷ レイヤーの大きさ」で求めていて割り切れない大きさでずれる（利用者の花 577×680 は 1/2・1/3 で該当の見込み）。(b) 群れの面を 24 px 単位でそろえていて 1/5・1/7 では画素の境目に乗らない。描画の縮小率を AE から直接得る方法はまだない（テクスチャのフックの第 1 引数はレンダーコンテキストではなかった。渡したら AE が落ちた）。この 2 件ではドラフトの有無で絵が変わる（どちらも正解とは違う）。
- **（11/25 21:05）(a)(b) は修正済み**（利用者の決定「おすすめの進行で」＝不具合が先、次に GPU 化）。描画の縮小率はキャンバスの関数表（版 10）の `AEGP_GetRenderDownsampleFactor` を差し替えて同じスレッドに覚える（`t_dsf`）。`run_merge.ps1 -Mfrs OFF,ON -CompareDraft -ForceD "200,150,300"` MERGE ALL PASS（158 s、ドラフトの有無 29 件一致）、REF ALL PASS、単体 ALL PASS（71）。**次の 1 手: GPU 化**（下の注意）。
- **（11/25 21:40）GPU 化は実装済み。** `GpuRasterizeHost`（`src/gpu/gpu_raster.cpp`）: GPU のプライマリーコンテキスト＋自前の非同期ストリーム、MFR では排他、AE の画素のまま送って GPU で形式を変換（`fast_raster.cu` の `ArgbToBgraF` / `BgraFToArgb`、丸めは CPU 版の Store と同じ式）、失敗したら CPU で描く。使う条件 `MergeGpuAllowed()`: AE がこのエフェクトに CUDA の装置を準備した（GPU 設定が CUDA）とき。試験用 `DUPLIX_MERGE_GPU_DRAW`（0/1）。**GPU 設定が「ソフトウェアのみ」のプロジェクトでは GPU を使わない。** 速さ（利用者のシーン、aerender MFR オフ）: 群れの絵 10 枚 40→6 ms、100 枚 89→11.5 ms。1 フレーム 10 枚 約 83 ms・100 枚 約 83 ms（コピー 0 枚 50 ms、通常 10 枚 233 ms・100 枚 1570 ms）。CPU の形式の変換が 30〜40 ms かかって最初は効かなかった（変換を GPU に移して解決）。試験 `run_merge.ps1 -Mfrs OFF,ON -CompareDraft -GpuDraw -ForceD "200,150,300"`（8 回分 292 s）: 全回合格、GPU と CPU の絵は 8 bit で最大 1 段階。16/32 bit の回は AE が 8 bit の TIFF にディザをかける（描くたびに 1 段階ずれる）ので 1 段階を許す（`check_merge.py` の `DITHER`）。
- 計画の残り: Task 7 の残り（画面＝run_ui の方式、300 フレームのメモリ）→ Task 8（状態の欄に理由・手引き・計測）→ 最終レビュー（台帳の Ruling を利用者に一覧で報告）。
- **（11/25 22 時）利用者が AE で新しい版を試し中（AE 起動中は AE の試験もリンクもできない。`DupliX.aex` が使用中で LNK1168）。** その間に Task 8 のコードと手引きを書いた（コンパイルは通った、リンクと試験はまだ）: 状態の欄に「軽量化は次の理由で効きません: …」（`MergeBlockNow` → `ReadMergeBlock`（`merge_scene.cpp`、`LayerBlock` / `ReadCompLights` に切り出し）→ `MergeBlockedText`）、ログ `status: comp C layer L merge block B`（`check_merge.py` の `check_scene` がビルド時の `plugin.log` で照合。UPDATE_PARAMS_UI が隠れた AE で届くかは未確認）、手引き `docs/duplix-quickstart.md` の「軽量化」の節。（21:45 済み: ビルドして試験一式は状態の照合以外すべて合格。隠れた AE には UPDATE_PARAMS_UI が届かず status の行が出ない → 照合は行があるときだけにした。）
- **（11/25 22:10）利用者の報告: `C:\Users\yakuruto\Desktop\DupliX Test A.aep`（コピーは `tests/ae/user/DupliX Test A.aep`）で軽量化をオンにすると前後が変わる所がある。** 再現: `pwsh tests/ae/run_user_merge.ps1 -Aep "tests/ae/user/DupliX Test A.aep" -At 0`（`user_merge.jsx`: コピーを開いて軽量化オン・オフのコンポを作って描く。`cmp_merge_frames.py` で比較・差の画像 `build/test/user_merge/diff_*.png`）。シーン: 青い花 380 枚（保存時は軽量化オフ）、家の 3D レイヤー、カメラは拡大縮小 241% のヌルの子、斜めから見る。計画は 3 群れ（286・7・7 枚）＋80 枚が 1 枚ずつ。差は家の右の x≈960〜1040 の数か所（5×5 平均の差の最大 202、>16 が 0.08%）。**分かったこと: 差の画素（985〜995, 765〜772）では AE（オフ）はコピー 227 を、軽量化はコピー 187（群れ 1）を手前に描く。2 枚は平行な面で、幾何では 187 が 23 単位手前（カメラから面までの距離 7020 と 7043。中心・角のどの物差しでも 187 が手前）。つまり AE のクラシック 3D の前後は、幾何と食い違うことがある。** 重なる組の全体では、はっきり判定できた 775 組のうち 754 組が幾何と一致（密集していて第三のコピーが混じるので、残り 21 組の多くは判定の誤りの可能性）。家と半透明のコピー 298 の縁（1010,530）でも差があり、どちらも AE が描くもの（群れの面を足すと AE の描く順が変わる？）。計画の中身は `DUPLIX_MERGE_DUMP=<file>` で書き出せる（`merge_cache.cpp` の `DumpPlan`、`MergePlan::debugCopies`）。調べるための使い捨てのスクリプトはスクラッチパッドの `plan_at.py` / `who.py` / `pairs.py`。**（22:20〜23:10）原因と対策:** 家をオフ／遠くにすると AE の前後は幾何どおりになり、軽量化と一致（`run_user_merge.ps1 -Disable house1.png` / `-MoveZ house1.png:3000`）。コピー 187・227 は家の平面をまたぎ、家もコピーの平面をまたぎ、画面上で重なるが 3D では交わらない。AE はこういう面どうしを幾何どおりに並べられず、その順は描く面の集まりで変わる（187・227 を両方 1 枚ずつ描いても、ほかのコピーを群れにすると入れ替わった）。**対策: そういう組（コピーとほかのレイヤーの面が画面で重なり、互いの平面をまたぎ、3D で交わらない）があるフレームは何もまとめない（`MergePlan::ambiguous`、`QuadsIntersect3D` を `merge_geom` に追加。本当に突き抜けるコピーは AE が正しく切るので従来どおりそのコピーだけ 1 枚ずつ）。** 結果: 利用者のシーン 8 フレームとも軽量化オン＝オフ（差 0。このシーンは家があるので軽量化は効かない）。単体 ALL PASS（73、`quads_intersect_3d`・`merge_ambiguous_no_merge` を追加）。**残る心配: コピー同士（回転の範囲がある場合）でも同じことが起きうる（未確認）。**
- **（23:15）利用者の問い「交わる部分以外をまとめる棲み分けはできないか」→ 案 2 を実験（コードは戻した）:** 問題のコピーと、画面上の重なりでつながるコピーを AE に任せ、残りをまとめる。結果（利用者のシーン、フレーム 0）: 連鎖すべて → 377/380 枚が AE、まとまるのは 3 枚（一致）。1 段まで → まとまるのは 6 枚程度、差が少し残る（5×5 平均の最大 26、>16 が 0.001%）。問題のコピーだけ → 元の不具合。**密集したシーンでは案 2 は役に立たない。** 試験一式（新しい規則）は MERGE ALL PASS（327 s）。**利用者に報告し、案 1（AE のクラシック 3D の並べ方を Advanced3D の解析で突き止める）に進むかを相談中。**
- **（23:30）利用者の指摘「奥行きが違えば実際に重なるコピーは少ないはず」「家とコピーは回転が違うので奥行きが違っても重なる」を受けた実験（コードは戻した）:** 連鎖を「画面と奥行きの範囲の両方で重なる組」に絞る → AE に任せるのは 2 枚、差（202）は残る。家と画面・奥行きで重なるコピーも起点に足す → 28 枚、差は残る。**決め手: コピー 187・227 は画面上で家と重なっていない（家の画面範囲 y 346〜732、2 枚は y 755 付近）のに、家の有無で AE の前後が変わる。AE は家の画像の範囲ではなく、家の無限の平面で空間を分けて順を決めているとみられ（平面で空間を分ける手法に似る、分け方が不正確）、その影響は画面の重なりの範囲に収まらない。** → 棲み分けの範囲は AE の分け方を解析しない限り決められない。**今の規則の穴: 家の平面をまたぐが画面で家と重ならないコピーだけのシーンは、規則に掛からずまとめるので、AE と前後がずれうる（未確認。ただし `wall_tilted` などの試験は一致している）。** 次: 利用者が A（解析）か B（計画の残りを先に）かを決める。週間枠 93%（11/26 8:00 リセット）。
- **（11/25 23:40〜11/26 0:30）利用者の決定 A（週間枠は気にしない）。Advanced3D（Escher）の解析の結果:** 全関数の逆コンパイルは `spike/ida/dump_a3d_all/`（`ida_dump_all.py`、Python は `A:/Disassemble/acnl/project_v2/tools/python/idalib/Scripts/python.exe`、i64 はコピーを使う）、機械語 `ida_disasm.py`、vtable `ida_vtables.py`。描画 `sub_180014470`: 面（232 バイトの項目、画面の範囲 +8..+0x20、動きのぼかしの時刻ごとの 192 バイトの表 +0xC8/+0xD0）について、`sub_18002AB40` が画面の範囲が重なる組ごとに `sub_18002B040`→`sub_18002B350`（画面座標の平面の式 +0x18..+0x30 で、重なる多角形の頂点の奥行きの差の符号。閾値 ±1e-12）で「どこかで手前」の辺を張り、`sub_18002A880`（Tarjan）で強連結成分に分ける。1 枚の成分はその順に描き（`sub_180014800`）、2 枚以上の塊は `sub_180013470` で断片の表（`SY_MBufferAA/Draft/MB`、画素ごとの連結リスト、奥行き float +0x0C と傾き +0x10/+0x14、合成は vtable +24）に描いて合成。**検証（スクラッチパッドの `aegraph.py` で同じ規則のグラフ）: 利用者のシーンの軽量化オフでは塊が 1 つ（12 枚、家と 187・227 を含む）。** 軽量化でコピーを抜くと塊が変わり、AE の描き方が変わって前後がずれた。**設計: (1) 全コピーを 1 枚ずつにした場面で同じグラフを作り、塊のコピーはまとめない。(2) まとめた後（群れの面＝余白を足した長方形を奥行き d に置いた面）でもグラフを作り、塊が同じで群れの面がどの塊にも入らないことを確かめる。満たさない群れは解いて計画し直す。今の「またぎ合う組があればフレーム全体をまとめない」はこれで置き換える。** 実装中。
- **（23:00）利用者の指示「ダイアログを検知できるように」**: 単体テストは確認画面を出さず stderr に書いて終了（`_CrtSetReportMode`、`SetErrorMode`、例外は `CRASH in test ...`。`UNIT_ASSERT_PROBE=1` で確認済み）。AE の試験は `run_jsx.ps1` が 2 秒ごとに見えないデスクトップの確認画面を探し、Win32 の標準ボタンを持つもの（メッセージボックス、C++ 実行時、エラー報告）があれば題名と本文を出して即失敗（`Get-AeHiddenDialogs`。AE の独自の窓は題名を記録するだけ）。**教訓（トラブル表へ）: 単体テストのビルド失敗を見ずに古い exe を実行し、確認画面で止めた（2 回）。ビルドの成否を見てから実行する。**
- 旧: **次の 1 手: 利用者の AE が閉じたら `pwsh tools/build.ps1` → `run_merge.ps1 -Mfrs OFF,ON -CompareDraft -GpuDraw -ForceD "200,150,300"`（scene_light の status を確認）→ `run_ui.ps1` → Task 7 の残り → Task 8 の計測（1000 枚も）→ 最終レビュー。**
- 旧: 利用者の判断（(a)(b) を先に直すか、GPU 化を先にするか）を聞く。GPU 化の注意（助言役）: 先に `RasterizeCopies` の「消すだけ」と全体の時間を比べて上限を測る、コンテキストは AE の CUDA のもの（GPU 設定が CUDA のときだけ）か自前か、自前の非同期ストリーム、MFR では排他、8/16/32 bpc の変換は CPU 版と同じ丸め、失敗したら CPU で描く。計測の道具: `frame_latency.py`（画面の 1 フレームの時間＝PNG の書き込み時刻−開始）、`cmp_frames.py`、`cmp_dirs.py`、`run_user.ps1 -RsTemplate`。残りの内訳: DupliX が群れの絵を CPU で描く時間（10 枚 38 ms、100 枚 87 ms）が次の候補（GPU 化）。
- Task 5・6 で分かったこと: 平面レイヤーは AE が絵を使わずに塗るのでまとめない（MB_SOLID）。絵を受け取るフックの時刻はレイヤーごとに覚える。群れ分けは 200 ms の上限（超えたらそのフレームはまとめない）。AE の前後の判定は 5×5 平均の差で行う（拡大したコピーは模様の境目に補間の差が格子状に残る）。
- 分かったこと: 速さの関門は合格（利用者のシーン 100 枚: 通常 1430 ms → 軽量化 200 ms／フレーム）。継ぎ目は出ない（Task 4）。1/2 解像度では AE の絵の「レイヤーの画素」が縮小済み。群れの中身の画質: DupliX の描画はバイリニアのみで、模様の境目（最大 43）と縮小したコピーの内側（ざらつき）で AE と差が出る（利用者に報告すること）。
- 試験: `pwsh tests/ae/run_merge.ps1 [-Cases ...] [-ForceD x,y,z]`（`check_merge.py`）、利用者のシーンの計測は `run_user.ps1 -Variants v_norm100,v_merge100,...`。

## 再開の手順（2026-11-25 正午、高速モードの GPU 化の途中で利用者の指示により中断。完了済み）

- **計画 `docs/superpowers/plans/2026-11-25-fast-mode-gpu.md`（設計書 `docs/superpowers/specs/2026-11-25-fast-mode-gpu-design.md`、承認済み）の Task 1〜4 は完了・push 済み（最後の実装の commit 5e44c6e）。Task 5 は途中**: 計測は済み、手引き（`docs/duplix-quickstart.md` の高速モードの節に速さの表）と開発記録（`docs/experiments/2026-11-25-dev-log.md` の 5 節）を更新して commit 済み。
- **残り: (1) 最終レビュー**（executing-plans の手順: `review-package` で差分一式を作り、別のレビュー役に見せる。範囲は計画の commit 6a82c29 から HEAD）→ 指摘の修正 → **(2) 利用者に AE で試してもらう**（今の `dev_plugins\DupliX.aex` は最新。ビルドの置き換えは利用者が AE を閉じているときだけ）。
- 進み具合と判断（Ruling）の台帳: `.superpowers/sdd/2026-11-25-fast-mode-gpu/progress.md`（git 管理外。最終レビューのあと、判断を開発記録に移して消す）。
- 中身: CPU 版は区画（16×16）ごとに、手前の点の近い順に処理し、不透明なかけらの奥は計算しない（結果は旧版と差 0）。GPU 版は CUDA のカーネル（`src/gpu/fast_raster.cu`）を CUDA 12.2 の `nvcc -ptx` で PTX にして埋め込み、`nvcuda.dll` から実行（`src/gpu/gpu_raster.*`）。AE の GPU 設定が CUDA のときだけ GPU、ほかは CPU。Furikake は実は CPU（Z バッファ）で描いていた。
- 速さ（利用者のシーン、書き出し、1 フレーム）: 通常 10 枚 183 ms。高速 CUDA 10/100/1000 枚 = 約 50/50/117 ms、高速 CPU = 約 100/133/333 ms（対応前 117/570/10 秒以上）。利用者のプロジェクトの GPU 設定はもともと CUDA。
- 確かめた試験: 単体 ALL PASS、`build/gpu/gpu_test.exe` GPU ALL PASS、`run_fast_mode.ps1`（CPU）と `-Gpu CUDA`（GPU の経路を使ったこと、CPU との一致）、`run_ref.ps1`（既定と `-Gpu CUDA`）、`run_ui.ps1`、`run_transfer.ps1`、fx_remove。
- ビルド: `pwsh tools/build.ps1`（エフェクト。先に PTX を作る）、`-Target gpu`（GPU の単体試験）。生成物 `src/gpu/fast_raster_ptx.h` は git 管理外。

## 再開の手順（2026-11-25 昼前、高速モードの作り直しの実装後）

- **計画 `docs/superpowers/plans/2026-11-25-fast-mode-plugin.md` の Task 1〜5 と最終レビューの修正は完了（ブランチ `feat/mvp`、commit 99a4c48、push 済み）。** 事実・判断・後回しにした軽微な指摘・トラブルは `docs/experiments/2026-11-25-dev-log.md`。
- 中身: ワールド変形のパラメータ（両モード、群れ ＝ レイヤー × ワールド変形）、高速モードは 2D レイヤーの普通のプラグイン（フックなし）、オンにした瞬間に現在フレームのレイヤーの変形をワールド変形へ写す（取り消し 1 回で戻る）、オフは 3D に戻すだけ。使い方は `docs/duplix-quickstart.md` の「ワールド変形」「高速モード」。
- 確かめた試験: 単体 ALL PASS（47）、`run_transfer.ps1` TRANSFER ALL PASS、`run_fast_mode.ps1` FAST ALL PASS（MFR オフ・オン）、`run_ref.ps1`、`run_ui.ps1`、fx_remove、`world_xform.jsx` WORLD ALL PASS、`run_as_is.ps1`（利用者のシーン）、`run_orbit_order.ps1 -Deg 26`。
- 最終レビューで直したもの: 位置の次元が分割されている・ワールド変形にキーフレームがある（レイヤーが初期状態でない）ときは、何も変えずに止めて「状態」で案内。カメラだけ動く場合の試験 `fast_camonly` を追加（問題なし）。
- **次: 利用者に AE で試してもらう**（今の `dev_plugins\DupliX.aex` は最新。ビルドの置き換えは利用者が AE を閉じているときだけ）。11/24 の高速モードで保存したプロジェクトは、高速モードを一度オフ → オンにし直すと新しい形になる（オンのまま開くと絵がずれる）。
- 注意: 軌道のアニメーションを 120 フレーム比べる `run_orbit_order.ps1`（角度指定なし）は、親のアニメーションを写さない決定により前提がなくなった（`-Deg` 付きだけ有効）。
- 次の作業の候補（利用者が順番を決める）: 複数の複製対象（設計書 12.13、高速モードのあと）、画質の選択、GPU での描画。保留は前のまま（まとめる最適化、シェイプ・テキスト対応、AE 2021/2026、改名、`dbg:` ログの整理、main への統合）。

## 再開の手順（2026-11-25 朝。完了済み）

- 利用者が確認済み: キャッシュの鍵の修正（963e092）で、軌道ヌル Y 26° のフル画質の前後が一致した。
- 高速モードの作り直しの計画を作成し、利用者が承認（このセッションで通しで実装）。利用者の決定（11/25）は計画の冒頭と設計書 12.13 の末尾。
- 複数の複製対象は高速モードのあと（利用者の決定）。

## 再開の手順（2026-11-25 0 時半、セッションをクリアする前に記録）

- **最初にすること: 利用者に、新しいビルド（`dev_plugins\DupliX.aex`、11/25 0:2x）で「軌道ヌルの Y 回転 26° のフル画質で、通常モードと高速モードの前後が一致するか」を確かめてもらう。** 報告されていた入れ替わりの原因は、AE のディスクキャッシュの鍵に混ぜるビルド ID が `duplix.cpp` のコンパイル時刻だけだったこと（`raster.cpp` だけの修正で ID が変わらず、AE が修正前の絵をキャッシュから出した。1/2 画質は未キャッシュで正しかった）。ID に aex 自身の更新時刻と大きさを足して修正（`src/effect/duplix.cpp` の `BuildId()`）。開発記録 #36。
- 利用者の実際のシーンは `tests/ae/user/DupliX Test.aep`（11/25 0:09 保存、軌道ヌル Y 26°、花のレイヤー 1 つに DupliX 10 枚）。これを手を加えずに描いて比べる試験 `pwsh tests/ae/run_as_is.ps1`（aerender とビューアーのフル・1/2）＝ AS-IS ALL PASS。**同じフォルダーの自動保存（20:56、4 レイヤー）は古い別の状態で、#34 ではこれを見誤った。**
- 試験の穴の教訓: 試験は毎回 `DUPLIX_CACHE_SALT` でキャッシュを避けるので、利用者の AE とは条件が違う。キャッシュの鍵に関わる変更のあとは、ソルトなしで「前のビルドで描いた絵が出ないか」も考える（`docs/ae-test-harness.md` に足すとよい）。
- 次の作業（利用者の確認後）: 高速モードの作り直し（設計書 12.13: 黒魔術なしの普通のプラグイン、2D レイヤー、コンプ大の出力、ワールド変形、オンにした瞬間にレイヤーの 3D の位置・回転をワールド変形へ自動で写す）の計画を writing-plans で立てる。複数の複製対象（掛けたレイヤーを含めるかのチェック＋追加の対象、取り込み方は 2 通りから選択、通常モードでも）は 12.13 に記録済みで、順番は利用者が決める（前回の質問に未回答）。

## 再開の手順（2026-11-24 深夜、高速モードの実装後）

- **高速モードは実装済み（計画 `docs/superpowers/plans/2026-11-24-fast-mode.md` の Task 1〜4 と最終レビューの修正。ブランチ `feat/mvp`）。次: 利用者に AE で試してもらう**（ビルドの置き換えは利用者が AE を閉じているときだけ。今の `dev_plugins\DupliX.aex` は最新）。使い方と失われるものは `docs/duplix-quickstart.md` の「高速モード」。
- （11/24 23 時）利用者の報告「軌道ヌルの Y 回転で前後が入れ替わる」を修正（画素ごとの奥行きで重ねる。commit 651b00a。再現試験 `tests/ae/run_orbit_order.ps1`）。A ドライブの空きがほぼない（試験の大きな書き出しは %TEMP% へ）。
- 確かめたこと: 単体テスト ALL PASS（43）、`run_fast_mode.ps1` FAST ALL PASS（9 ケース、MFR オフ・オン）、`run_ref.ps1` REF ALL PASS、`fx_remove`・`run_ui.ps1` 合格（高速モードはオフ）。
- 速さ（利用者のシーン、aerender MFR オフ、CPU が他の作業で約 60% 使われた状態）: 通常 10 枚 150 ms、高速 10 枚 117 ms、100 枚 267 ms、1000 枚 2.2 秒。通常は 1 枚約 14 ms なので枚数が多いほど差が開くが、計画の「10 枚で半分以下」「1000 枚が 10 枚の 3 倍以内」は未達（自前の描画がコピーの覆う画素数に比例する）。次の高速化の候補は GPU（CUDA）か、CPU のさらなる最適化。**CUDA Toolkit は導入済み**（v13.3＝`CUDA_PATH`、v12.2 も。nvcc・NVRTC あり。RTX 3060、ドライバー 596.36。Everything 1.5a は `C:\Tools\es.exe -instance 1.5a` で検索できる）。
- 判断の一覧（計画からの逸脱を含む）と、後回しにした軽微な指摘は `docs/superpowers/plans/2026-11-24-fast-mode-rulings.md`。主なもの: AE のエフェクトの画素はストレートアルファ（計画はプリマルチプライと書いていた）、面の表はレイヤー時間で鍵・64 件・最も近い時刻、2 画面表示の制限、`fast_identity` の判定の変更。
- 計測の注意: AE のディスクキャッシュは同じビルド・同じ内容の絵を返す。`run_user.ps1` は毎回 `DUPLIX_CACHE_SALT` を変える。
- 保留（前のまま）: まとめる最適化（12.7 の (3)）、シェイプ・テキスト対応、AE 2021/2026 で落ちる件、12.5 の省く判定、改名（DupliX）、Final Review と `dbg:` ログの整理、main への統合。

## 再開の手順（2026-11-24 夜、高速モードの実装前。完了済み）

- **次の作業: 高速モードの実装。計画 `docs/superpowers/plans/2026-11-24-fast-mode.md`（Task 1〜4）を、このセッションで直接、通しで実装する（利用者の指示「通しで実装します」＝ executing-plans の方式）。** 設計は設計書の 12.10〜12.12（承認済み）。ブランチは `feat/mvp`（push 済み、最新 commit は `git log -1`）。
- 最初に読むもの: (1) 計画、(2) 設計書の第 12 節（12.4〜12.12。特に 12.10 の実験の事実と 12.11・12.12 の決定）、(3) 今日の記録 `docs/experiments/2026-11-24-dev-log.md`（試験の一覧、修正した不具合、測り方、小さな失敗）、(4) `docs/ae-test-harness.md` の 4.7〜4.9（速さの測り方、利用者のファイルを壊さない、AE の内部確認）。
- 高速モードの要点: 全コピーを DupliX が CPU で 1 枚の絵に描き、カメラ正面の 1 枚の面（M4 = S·C·W⁻¹）として画面に 1 対 1 に重ねる。置き方は実験で画素一致済み（`DUPLIX_SPIKE_SCREEN=1` の実験コードは Task 3 で製品コードに置き換えて消す）。面の奥行きはアンカー、ライトは無視して案内、後ろのエフェクトは 1 枚の絵に掛かる、AE と画素一致はしなくてよい（縁の 1〜2 段階の差を許す）。GPU（CUDA）は CUDA Toolkit の導入を利用者が判断してから別計画。
- 守ること: 前処理のすべての分岐で `GuidMixInPtr` を呼ぶ／`result_rect` は要求との共通部分・全体は `max_result_rect`／新しいパラメータは末尾／利用者の aep（`tests/ae/user/`）は必ずコピーを開く／AE を動かす試験の前に AfterFX・aerender が動いていないことを確かめる（利用者の AE を巻き込まない。ビルドの置き換えも利用者が AE を閉じているときだけ）。
- 高速モードのあと: まとめる最適化（12.7 の (3)、高速モードの部品を使う）。保留中: シェイプ・テキストへの対応、AE 2021/2026 で落ちる件（知人のログ待ち）、12.5 の省く判定、GPU の差し替え（最終手段）、改名（DupliX、第 12.1 節。利用者の確認後）、Final Review と `dbg:` ログの整理、main への統合。
- 速さの基準値（利用者のシーン、1 フレーム）: 通常のコピー 10 枚 142 ms、本物 10 枚 142 ms、画面全体の面 1 枚 58 ms。測り方は `pwsh tests/ae/run_user.ps1 -Aep "tests/ae/user/DupliX Test.aep" -Mfr OFF -Variants ...`（「合計経過時間」÷ フレーム数）と `run_preview.ps1`。

（以下は古い順に積み上がった記録。上ほど新しい）

**（11/24 11 時）Task 7 完了（commit 02b7010、push 済み）。`pwsh tests/ae/run_ui.ps1` → UI ALL PASS（ビューアーを開いて 30 フレームのスクラブ、コピー数・種・回転の変更、取り消し、fx オフ＝エフェクトなしの複製と画素一致、fx オン、キーフレームした回転の範囲で UI の絵＝aerender）。`run_ref.ps1` も全ケース合格。利用者向けの手引き `docs/duplix-quickstart.md`。未確認: ビューアーを手で操作したときのメインスレッドの経路（`-r` のスクリプトは終わると AE が終了するので自動では試せず、メインスレッドからの問い合わせは 0 回だった）。**次: 利用者に AE で試してもらう（ビルドの置き換えは利用者が AE を閉じているときだけ）→ 結果を聞く → executing-plans の Final Review → `src/` の `dbg:` ログの整理 → main への統合。**
**（11/24 18 時）重さの調査: 利用者の「タイムラインのレンダリング時間がコピー 10 枚で約 300 ms」を受けて計測方法を変えた。aerender の「合計時間÷フレーム数」は MFR で複数フレームを同時に描くので 1 フレームの時間ではなかった（誤り）。新しい計測: `tests/ae/run_preview.ps1`（AE を起動したままにして `AfterFX.exe -r` でスクリプトを送る。起動完了前に送ると止まる。カメラにエクスプレッションで動きを付けて毎フレーム描き直させる＝AE は同じ内容のフレームを使い回す）＋プラグイン内の 1 フレームごとの計測（`DUPLIX_TIMING=1`、`src/hook/timing.*`）。直したもの: `IsLayerType` のフックを照合済みアドレスの比較だけに（UI で 64 万回呼ばれていた）、AEGP の読み取りの使い回し、前処理（1 フレームに約 23 回）の近道。結果（利用者のシーン、ビューアー経路）: DupliX 10 枚 141 ms ≒ 本物 10 枚 140 ms、プラグインの処理 4 ms。残りは AE のクラシック 3D が面 1 枚を描く時間（このシーンで約 14 ms/枚）。次: 利用者に今の版で同じ列を測り直してもらい、手で 10 枚複製した場合とも比べてもらう → その結果で 12.5（省く判定。ただし利用者のシーンではコピーがほぼ画面内なので効かない見込み）と 12.7（1 枚の絵にまとめる・高速モード）の順番を決める。利用者の aep は `tests/ae/user/`（Git 管理外）。必ずコピーを開く（AE は閉じるときに無確認で保存する）。**
**（11/24）利用者の報告: シェイプレイヤーとテキストレイヤーに掛けても複製されない。今の版は AV レイヤー（`BEE_AVLayer` の vtable）だけが対象。シェイプは別の vtable で、実験（試験 6・7）では差し替えで複製できた → 対応できる見込み。テキストは文字単位 3D のための独自のサブレイヤーと別の構造を持つので要調査。まだ着手していない。**
**（11/24 16 時）Advanced 3D のコンプで落ちる問題を修正（クラシック 3D 以外ではコピーを出さない）。位置の範囲の表示が 255 倍だった問題を修正。計測: 書き出しではコピーの描画は軽い（設計書 12.8）。利用者が重いと感じた場面はまだ聞けていない。AE 2021・2026 で知人の環境が落ちた件: 2021 は差し替えが入らないはずなので、原因は差し替え以外（エフェクトの種類の番号を求める処理が差し替えなしでも動く点が怪しい）。2021 版の DLL 4 つは `spike/ida/versions/2021/`、照合は `spike/ida/abi_check.py`。ログ（`DUPLIX_LOG`）を知人に頼む。2021 対応は 2025 が一段落してから。**
**（11/24 14 時）利用者の報告「エフェクトを付けて外すとレイヤーの表示が壊れる」を修正（AE はエフェクトの削除・fx スイッチ・3D スイッチの変更でエフェクトにコマンドを送らず、登録の写しが古いまま残っていた。フックの判定に、BEE/FLT から直接読む「3D・このエフェクトが付いていてオン」を追加。試験 `tests/ae/fx_remove.jsx`＋`check_fx_remove.py`）。GPU 対応（CUDA/OpenCL/DirectX 12 のメモリコピー、`src/effect/gpu_copy.cpp`。この PC の AE では CUDA しか選べず、CUDA で画素一致を確認。`run_ref.ps1 -Gpu CUDA`）。設計書 第 12 節に 12.4 見送り・12.5 省く判定・12.6（深度テスト後のコピー全体へのエフェクト、(a) で調査→将来 (b)）を追記。次: 利用者が今のプラグインをテスト → 12.6 の調査。**
**（11/24）次の版の要望（改名・英語化・開発者名とカテゴリ・GPU と高速化・インスタンスの最適化・画面外とエフェクトの保証・2 つのレイヤーの交差の調査）を設計書 第 12 節に記録。着手は、最初の版を利用者が確認してから。**

**（11/24 06:00）試験 23（コピーごとの不透明度）まで成功。**
**（11/24）利用者の決定: サブレイヤー方式・回転はアンカーポイント中心・散らすのはレイヤー座標・枚数の上限なし（重さは説明）・ばらつきは位置/回転/拡大縮小/種/不透明度（時間のずらしは将来）。詳細は `docs/decisions-2026-11-24.md` の冒頭。利用者は早く自分で試したい。次は設計書をサブレイヤー方式に書き直して承認 → writing-plans → 実装。**
**（11/24 実装中）ブランチ `feat/mvp` で計画を実行中（このセッションで直接実装）。進み具合は `.superpowers/sdd/2026-11-24-duplix-sublayer-mvp/progress.md`（git 管理外）と `git log`。Task 1〜6 完了（11/24 10 時前、5 時間枠 98% で区切り。リセット 10:50）。Task 6: `pwsh tests/ae/run_ref.ps1` で全ケース（拡大縮小・アンカー・対象自身の回転/拡大縮小/親・不透明度・1 枚目固定・PNG＋エフェクト・影/被写界深度/調整レイヤー/モーションブラー・100 枚・0 枚・エフェクトのオフ/削除・コンプ複製）が aerender の MFR オフ・オンと開き直しで正解と差 0〜1/255。**次は Task 7**（`tests/ae/ui_session.jsx`・`run_ui.ps1` で UI 操作中に止まらないか＋フックのスレッド集計、`docs/duplix-quickstart.md`、利用者に試してもらう）。その前に `tests/ae/ref_cases.json` の `hyp_x2`・`hyp_xneg` を消す（仮説確認の名残）。Task 7 のあとは最終レビュー（executing-plans スキルの Final Review）。`src/` の `dbg:` ログは UI 試験のスレッド集計に使ってから整理する。分かったこと: SEQUENCE_SETUP 中に AEGP でエフェクトを読むと追加が失敗する／パラメータ名は CP932 で渡す／フックの鍵（BEE の comp ID）は AEGP の項目 ID と一致。**
**（11/24）公開リポジトリ https://github.com/yak9909/DupliX （main）。push は利用者に確認せずに行ってよい（利用者の指示）。`spike/ida` の `BEE.dll`・`.i64`・逆コンパイルの書き出し、`spike/logs`、`.claude/` は `.gitignore` で除外（Adobe のバイナリと解析結果は公開しない）。README は natural-japanese スキルで書いた。**
**（11/24）設計書 v2 は利用者が承認済み。2D レイヤーへの適用は将来可能にする（最初の版は案内のみ）。実装計画: `docs/superpowers/plans/2026-11-24-duplix-sublayer-mvp.md`（Task 1〜7。利用者の確認と実行方法の選択待ち）。フックの中の鍵は「コンプの項目 ID＋レイヤー ID」（`BEE_Layer::GetParentComp`→`BEE_Item::GetID`、公開関数あり）。**
**（11/24）設計書 v2 を作成: `docs/superpowers/specs/2026-11-24-duplix-sublayer-design.md`（承認待ち。第 6 節の 2 問は回答済み: 元のレイヤーは「コピーだけ＋1 枚目は元の位置のチェック」、2D レイヤーは「コピーを出さず 3D にするよう案内」）。承認後は writing-plans で、最初の区切りを「UI で触れる最小の版」にする。利用者に渡す前の必須確認は設計書 第 9 節の上 4 つ（UI プレビューで止まらない・拡大縮小とアンカー・読み込み時の自己登録・コンプ複製での ID 重複）。**
**（11/24 早朝、旧）試験 22 まで完了、試験 23 の 2 回目はビルド済み・未実行だった。利用者向けの判断材料は `docs/decisions-2026-11-24.md`。以下は旧見出し。**
**（11/24）試験 18 まで: 平面・PNG 素材（エフェクト付き）のコピーが、位置・回転・陰影・影・交差・被写界深度・調整レイヤー・モーションブラーで正解と画素単位で一致。aerender（MFR オフ・オン）でも一致。キャッシュはエフェクトのパラメータ変更で正しく更新、hook から複製レイヤーのパラメータも読める。再開したら「試験 15」の下の「次の 1 手」の 4（設計の相談）と「その先」から。利用者は就寝中: 中断するときは必ずこのメモを更新してクリアしてよい状態にする。** 見張りの閾値は全プロジェクト共通で 95% に変更済み。
- 利用者への説明済み: レイヤーセットはエフェクトではなく、レイヤー自身のプロパティのグループ（トランスフォームやマスクと同じ並び）。未公開で、作りかけの可能性もある。

## まず読むもの
1. `docs/experiments/2026-11-22-spike-log.md` … これまでに試したことの全記録（環境、失敗の原因、分かったこと）
2. `docs/superpowers/specs/2026-11-22-duplix-depth-scatter-design.md` … 設計書（下書き）
3. `docs/ae-test-harness.md` … 自動テスト環境の使い方と注意点（ほかのプロジェクトでも使う）

## 利用者が決めたこと（蒸し返さない）
- 作るもの: AE 2025 用のエフェクト。適用したレイヤーの画像を XYZ にランダムに n 個（上限なし）配置し、コンプ内の**全レイヤー**との前後を画素単位で判定する（深度テスト）。
- **1 レイヤーに 1 エフェクトで絶対に完結させる**。補助レイヤー、複製、他レイヤーを隠す運用は不可。必要なら「黒魔術」（非公式な手段）も許容。
- 方式: エフェクト内部で深度を持ち、自前で合成する。具体的には、設計書の「くり抜き方式（A'）」で進めている（エフェクトのレイヤーを最上段に置き、他レイヤーより奥の部分のアルファを削る）。
- ~~他レイヤーの取得: 隠しレイヤーパラメータ（スロット）＋エクスプレッション。~~ → **（11/23 夕方 決定）スロットとエクスプレッションをやめ、他レイヤーの絵・位置・深度・不透明度・表示状態はすべて AEGP で取る**（S5b の結果による）。スロット数の上限はなくなり、S3 は不要。
- **絶対条件（11/23 確定、設計書 第 2 節）**:
  1. ランダム配置をするレイヤーにエフェクトを 1 つ掛けるだけで成立する（効果プリセットや手作業の設定も不可）。
  2. 隠れは必ず AE のレンダリング結果をもとに判定する。他レイヤーのマスク・エフェクト・トラックマットを必ず反映する。
- **他レイヤーの画素は AEGP で描画させて取る（案 3、11/23 決定、設計書 4.2 節）**。`PF_Param_LAYER` のチェックアウトは「ソース」でマスク等を含まないため不可。
- 検証は Claude が自動で行う（ビルド → シーン生成 → aerender → 画像判定）。ホットリロードも欲しい（UI で調整する段階で導入する予定）。

## 進め方の状態
- brainstorming スキルの途中。方式は選択済み。設計書は下書きで、**未承認**。
- 実験の結果を設計書に反映し、5.4 節の未決事項（エフェクトより上のレイヤー、2D レイヤー、平面でないもの、コピーの向きやばらつき）を利用者と決めて、承認をもらう。承認後に writing-plans スキルで実装計画を立てる。
- 実験コード（`spike/`）は使い捨て。製品コードには流用しない。
- 利用者への返事はすべて日本語で書く。

## 自動実行の決まり（11/23 追加。利用者の指示）
- 利用者はテスト中も PC で作業している。AE は見えないデスクトップで動かす（`spike/tools/ae_bg.ps1`）。利用者の画面を撮らない。撮影は AE のウィンドウだけを PrintWindow で撮る。
- ダイアログの判定に OCR などの不安定な方式を使わない。内部の状態（レジストリなど）で判定する。
- クラッシュ修復ダイアログは、起動前に `Repair-AeCrashState`（`spike/tools/ae_guard.ps1`）で防ぐ。詳細は実験記録 5.1 節。

## 状況
- 設計書（下書き）: `docs/superpowers/specs/2026-11-22-duplix-depth-scatter-design.md`。実験結果の反映と利用者の承認はまだ。
- 実験コード（使い捨て）: `spike/`。ビルドは `spike/tools/build.ps1`、実行と自動判定は `spike/tools/run_s1.ps1`。
- プラグインの配置: `MediaCore\duplix_dev` → `DupliX\dev_plugins` のジャンクション（作成済み）。

## 実験で分かったこと
- S1 合格: 小さな 2D レイヤーのエフェクトが、コンプ全体の出力をコンプ座標に正確に合わせられる（移動・縮小・回転・アンカー変更の 4 種）。出力ワールドの `origin_x/origin_y` が、結果の矩形の原点と一致する。回転・拡大したレイヤーでは、AE の再サンプリングで縁に 1px 程度のにじみが出る。
- S2 一部確認: レイヤーパラメータを 64 個定義できる。エクスプレッションで `effect('DupliXSpike')('Slot 1')` から Layer を参照でき、`toComp()` でカメラ投影後の画面座標が取れる。`activeCamera.fromWorld(L.toWorld(p))[2]` でカメラ空間の深度が取れる。チェックアウトはレイヤー座標の画素で、空のスロットは 0×0。
- S2 の形の一致判定（透視補正つき）は未確認。直前の実行は下の AE の問題で止まった。
- 重要: AE のディスクキャッシュは再起動しても残り、プラグインを作り直しても古い絵を返す。`PF_OutFlag2_I_MIX_GUID_DEPENDENCIES` を立て、PreRender でビルド ID を `GuidMixInPtr` に混ぜて対策した（ビルド済み、未検証）。本番にも必要。
- AE の自動実行の注意:
  - `-noui -r` ではスクリプトが動かない。`AfterFX.exe -r` を使う。
  - `-r` に渡すパスに空白を含められないので、%TEMP% にコピーしてから渡す。
  - `app.quit()` が効かないことがあるので、ログの完了行を見てから実行側で AE を閉じる。
  - 出力テンプレートに PNG がないので、TIFF を使う。
  - Python は `spike\.venv`（uv で作成。PATH の python は MSYS2 版で使えない）。
- テストコンプのレンダラーは `ADBE Advanced 3d`（＝クラシック 3D）だった。

## 解決済み（11/23）
- 強制終了後にスクリプトが動かなかった件: 原因は「クラッシュ修復オプション」ダイアログ。起動前にレジストリの印を書き換えて防ぐ。
- `-r` のパスを引用符で囲むと何も実行されない（AfterFX.exe は引用符を処理しない）。
- `run_s1.ps1` の再実行: キャッシュ対策は効いた（SmartRender 4 回）。S1 は全合格。S2 は v2_anchor だけ IoU 0.9899 で、判定の 0.99 をわずかに下回る（原因は未調査）。

- S2 合格: v2_anchor の差は縁の誤差だった。判定を「食い違いがすべて縁から 2px 以内」に変えた（実験記録 7 節）。
- （決定 11/23）今の方式（見えないデスクトップの AE）のまま進める。nexrender の方式は採らない。

## S2b の結論（11/23、実験記録 7.1 節）
- レイヤーパラメータの既定の取り込み方（「ソース」とみられる）では、対象レイヤーのマスク・エフェクト・トラックマットはチェックアウトの結果に含まれない。
- テキスト・シェイプ・コラップスしたプリコンプは、3D 変換とカメラ投影を済ませたコンプ座標の絵で返る（平面・素材・プリコンプはレイヤー座標）。
- `check_s1.py` は、これらの「分かっている制限」の 6 種を (info) として出し、合否から外した。

## S5 の結論（11/23 午前、実験記録 7.2 節）
- 描画中（PreRender・SmartRender のどちらでも）に `AEGP_RenderAndCheckoutLayerFrame` で他レイヤーを描画できる。MFR オン／オフ・aerender で、デッドロックもクラッシュもなし。
- 受け取りの GUID を PreRender で `GuidMixInPtr` に混ぜると、対象ごとに別のキャッシュになる。
- マスクは反映される。テキスト・シェイプはコンプ座標の投影済みの絵で返る（そのまま使えば一致）。
- **反映されないもの（利用者に相談中）**: トラックマット／レイヤーの範囲からはみ出すエフェクトの結果（範囲で切られる）。
- コラップスしたプリコンプ: AEGP では画素なし（world が NULL）。スロットのチェックアウト（コンプ座標の絵）を使えば形は合格。ただしそのレイヤー自身のマスク・エフェクトは入らない見込み。
- キャッシュの注意: 受け取りの GUID は中身の変化しか拾わない見込み。対象の移動・カメラの変化は、今はエクスプレッションの角の位置が拾っている。位置も AEGP に移すなら、変換・カメラ・レイヤー一覧を自分で混ぜる（S5b の候補）。
- 自動テストの改善: エフェクトが原因のクラッシュで次の起動時に出るダイアログは `Effect Crashed.txt` が原因。`Remove-AeEffectCrashRecord` で防ぐ（見本のファイルでは確認、実物では未検証）。aerender に待ち時間の上限を付けた。
- 実行: `spike/tools/run_s1.ps1 -S5Site pre|smart -Mfr ON|OFF`。4 通りの通し実行はスクラッチパッドの使い捨てスクリプトで行った。

## 利用者の決定（11/23 午前）
- トラックマット: **案 A に決定**。マットのレイヤーも AEGP で描画し（`AEGP_GetTrackMatteLayer`）、マットの種類に従ってコンプ座標で掛け合わせる。
- はみ出すエフェクト（他レイヤーに掛かったドロップシャドウなど）: **案 2 に決定**。範囲を広げる方法を探す（黒魔術・非公式な手段も許容）。`AEGP_LayerRenderOptionsSuite2` には ROI の設定関数がないので、別の手段を調べる（例: `AEGP_RenderOptionsSuite4`（アイテム用）に ROI の設定があるので、コンプ単位の描画やレイヤー用オプションとの併用、ネットや SDK サンプルでの先行例、ヘッダの未公開の関数）。見つからないときだけ、制限として書く案に戻して利用者に相談する。

## S5c の結論（11/23 昼、実験記録 7.3 節）
- **はみ出すエフェクトは解決**。`AEGP_LayerRenderOptionsH` は AE 内部の `BEE_LayerRenderOptions*` そのもので、`BEE.dll` が公開する `BEE_RenderOptions::SetROI`／`GetROI`（`M_Rect` = int16 ×4 `{top,left,bottom,right}`、コンプ座標）で範囲を広げられる。t_effect は IoU 0.757 → 0.9938。
- **コラップスしたプリコンプも AEGP で取れるようになった**（ROI を ±約 3 万に広げたとき。コンプ座標の絵で返り、IoU 0.9912）。`AEGP_LayerFlag_COLLAPSE` でコンプ座標として扱う。
- `zero`（{0,0,0,0}）は「空」で不可。MFR オン／オフとも同じ結果。クラッシュなし。
- 実験コードは環境変数 `DUPLIX_S5C_ROI=margin:N` で切り替え（未設定なら既定の範囲）。
- 本番の未決: 範囲の上限の決め方（巨大な結果を返すエフェクト対策）と、関数が見つからない版の AE での扱い。

## S5c のあとに分かったこと・したこと（11/23 昼）
- 範囲の拡大は**既定でオン**にした（環境変数が未設定なら `margin:30000`。`DUPLIX_S5C_ROI=none` で既定の範囲に戻る）。
- `check_s1.py`: `t_collapse` を合否に入れた（AEGP 経由を必須）。`t_text` は細い字なので、IoU の下限を 0.80 に下げた（縁から 2px の規則は同じ）。この状態の基準の実行では、**不合格は `t_matte` だけ**だった。
- `BEE.dll` の公開関数を `dumpbin` で調べた。`BEE_LayerRenderOptions::SetLayerStage`（既定 3）を 4・5 に変えても、トラックマットは掛からなかった（マットはコンプでの合成で掛かる）。黒魔術での近道はここで打ち切り、案 A に進んだ。

## S6（トラックマット、案 A）: **合格**（11/23 午後、実験記録 7.4 節）
- マットの 4 種（アルファ／反転アルファ／ルミナンス／反転ルミナンス）がすべて `render=aegp` で合格。ほかの項目も崩れず ALL OK。
- マットのレイヤー（ビデオがオフ）も AEGP で画素を返す。
- シーン生成の不具合は解決: 1 個目の出力モジュールのテンプレートを `RNDM_TIFF_A`（ASCII 名）として保存し、2 個目以降はそれを適用する。`setSettings` で写す方式は形式が写らず、2 個目以降が H.264 のままだった。
- 未確認: マットの入れ子、マットのアニメーション（S4 でまとめて見る）。

### （以下は作業途中だったときの記録）
- 実験コードは実装済み・ビルド済み:
  - パラメータ `S2 C0..C2` を末尾に追加した（Slot 2 のレイヤーの角の位置）。
  - PreRender で `AEGP_GetTrackMatteLayer`／`AEGP_GetLayerTransferMode` を使い、マットのレイヤーも `RenderTarget` で描画する（ログは `S6 matte:`）。
  - SmartRender で「Target のアルファ × マットの値（アルファ／反転アルファ／ルミナンス／反転ルミナンス）」を求める（`SampleAtComp`）。
- `make_s1.jsx`: `Matte` という名前のレイヤーがあれば、Slot 2 とエクスプレッションを設定する。`t_matte`（アルファ）、`t_matte_inv`、`t_matte_luma`、`t_matte_nluma`（白と黒の四角を並べたシェイプ）を追加した。
- **止まっている所**: コンプを増やしたら、既知の「テンプレート名が化ける」問題でシーンの生成が止まった。2 個目以降の出力モジュールに 1 個目の設定を `getSettings`／`setSettings` で写す方式に変えたところ、シーンはできたが、**出力の TIFF が 1 枚（v0_identity）しかできなかった**（写した設定に出力先が含まれ、そのあとの `om.file` の指定が効いていないとみられる。未調査）。

## S4・S5b: **合格**（11/23 午後、実験記録 7.5 節）
- 同じ AE のセッションの中で「描く → 変える → 描く」を行う試験を作った（`run_s4.ps1`。`-NoGuid`／`-S5b 1|compare`／`-NoSlot`／`-NoMix` で条件を切り替え）。変更なしの 2 回目はキャッシュから返る（試験として有効）。対照実験で、GUID を混ぜないと古い絵が返ることも確認した。
- マスク・エフェクト・プリコンプの中身・マットのマスク・移動・カメラの変更は、すべて再描画されて正解と一致。
- 不透明度は AEGP の絵に入らない → `AEGP_StreamSuite6` で読んで自前で掛ける方式で解決（マットの不透明度も）。
- **S5b: 位置・深度は AEGP の行列（`GetLayerToWorldXform`＋`GetEffectCameraMatrix`）で、エクスプレッションと小数第 4 位まで同じ値が出る**。1 回 0.6ms。エクスプレッションは 1 フレームあたり 300〜690ms かかっていた。AEGP の描画そのものは 1 フレームあたり 3〜13ms（PreRender の合計の大半は実験用のログ）。
- **スロットもエクスプレッションも使わない条件（本番に近い）で、S4 の全ケース（`-S5b 1 -NoSlot`）と、スロットなしで作り直したシーンの S1 全 18 項目（`DUPLIX_S1_NOSLOT=1`＋`DUPLIX_S5B=1`）が合格**。計算した位置は鍵に混ぜる必要がある（混ぜないと、自分のレイヤーの変形だけが違うコンプで別のコンプのキャッシュが流用された）。
- 表示されないレイヤー（目のスイッチがオフ・時間の範囲外）も AEGP は画素を返すので、`AEGP_IsVideoActive` が偽なら不透明度 0 として扱う（`t_hidden`、`t_outrange` で合格）。カメラのないコンプも合格（`t_nocam`）。
- **含意（利用者に確認したい）**: スロット＋エクスプレッションをやめて、全部 AEGP で取る方式にできる。そうすると S3（スロットの自動割り当て）は不要になり、スロット数の上限もなくなる（「将来は上限なし」がすぐ実現できる）。
- 未確認: 画面のプレビューでの挙動、レイヤーの追加・削除・並べ替え（レイヤー一覧を鍵に混ぜる）、合成の段階で掛かるほかの性質（対象より上の調整レイヤー、描画モード、レイヤースタイル、ソロ）、`AEGP_SetTime` の時間の種類、MFR の切り替え。

## S7（全レイヤーを対象にする）: **合格**（11/23 夕方、実験記録 7.6 節）
- `DUPLIX_S7=1` で、コンプの全レイヤーを対象にする（重ね絵は和集合）。対象から外すもの: 自分、カメラ・ライト、ヌル、ガイド、調整レイヤー、ソロ以外（ソロがあるとき）、`IsVideoActive` が偽、このエフェクトを持つレイヤー、実験用の `BG`。
- S1（スロットなしのシーン、24 項目）全合格。S4（`run_s4.ps1 -S7 -S5b 1`）で表示の切り替え・追加・削除・並べ替え・49 枚を含む全 12 ケース合格。
- 49 枚: AEGP 部分は 1 フレームあたり約 300ms（PreRender 3 回分。ログ込み、未最適化）。
- 再入の防止は実装したが、実際に働く場面（別のエフェクトがこちらをエフェクト込みで取り込む）は未検証。
- シーン生成で 1 回、AE が起動中（フォントの読み込み中）に止まった。再実行で直った（たまたまとみられる）。

## （11/23 夜）くり抜き方式（A'）は不採用。正しい見た目と完全一致が必須
- 比較画像（`spike/run_approx.ps1` → `spike/logs/approx/approx_compare.png`。AE の標準機能だけで正解と A' を描き比べた）を見た利用者の判断: 「絶対駄目。絵の改変に等しい。絶対正しい見た目にする。軽量化も考える」。半透明・ぼかしの後ろで背景が透け、加算・乗算の後ろでコピーが消えていた。
- **「正しい」の定義（案）**: 各コピーを本物の 3D レイヤー（元レイヤーの複製）として同じ変形で置いた参照コンプを AE が描いた絵と、RGB(A) で画素ごとに一致（8 bpc で ±2/255 程度）。形だけの判定は合格に数えない。
- **新しい方式（案）: コピーがある画素だけを全部合成し直す**。コピーのない画素は透明を出す（AE の合成がそのまま見えるので正確で、費用もゼロ）。コピーのある画素では、各レイヤーの AEGP の絵とコピーを、AE の規則（順序・描画モード・不透明度・マット）で自前で合成し、不透明で出す。コピーの範囲に掛かるレイヤーだけ描画し、同じフレームの 3 回の PreRender で結果を使い回す。
- 実験の順: (a) 背景の上のコピー 1 枚が、本物の 3D レイヤーと RGB で一致するか（透視の再サンプリング・縁・プリマルチプライ）→ (b) `make_approx.jsx` の正解と一致するか → (c) 描画モードを 1 つずつ → (d) 3D の重なり順の規則 → (e) 背景のないコンプ。
- 再合成では再現が難しいとみられるもの（利用者に必須か対象外かを聞く）: コピーより上の調整レイヤー、クラシック 3D のライトと影、カメラの被写界深度、モーションブラー、Advanced 3D／Cinema 4D レンダラー、環境レイヤー、レイヤースタイル。

### 利用者の回答（11/23 夜）
- 「正しい」の定義（上の案）: **承認**。
- コピーに必ず正しく効く必要があるもの: **調整レイヤー、ライトと影、被写界深度・モーションブラー**（必須）。Advanced 3D／Cinema 4D レンダラーは当面対象外。
- これらは AE が合成の段階で掛けるので、上の「自前で合成し直す」方式では実現できない（AE の 3D レンダラーの作り直しになる）。→ **コピーを本物のレイヤーとして AE 自身に描かせる必要がある**。置き場所の候補: (A) エフェクトが管理する隠しコンプ（プロジェクトに項目が見える・同期が要る・描画が約 2 倍）、(B) BEE の内部に一時レイヤーを差し込む（プロジェクトは変わらないが実現性不明・危険）。どちらも「補助レイヤー・複製は不可」の条件とぶつかるので、**利用者が選ぶ**。その前に実現性を短く確かめる: (i) PreRender から別のコンプを AEGP で描けるか（デッドロック・費用）、(ii) エフェクトがメインスレッドで動く機会と、そこでの編集が取り消し履歴・未保存の印に影響するか、(iii) 隠しコンプに「コピー＋利用者のコンプをコラップスしたプリコンプ」を置いて、ライト・影・調整レイヤー・被写界深度が正解と RGB で一致するか。

### （11/23 夜）利用者の指示「実装の前に立ち止まり、もっと楽な方法を探す（ネット検索・検証・AE の静的解析）」→ 調査中
- **有力な候補: BEE の「サブレイヤー」の仕組みに乗る**。文字単位 3D のテキストは、1 つのレイヤーの中に文字ごとのサブレイヤーを持ち、クラシック 3D で本物の 3D の板として描かれる（ライト・影・被写界深度・モーションブラー・交差がすべて効く）。`BEE.dll` は、テキスト専用ではない `BEE_AVLayer` の仮想関数として、`SubLayerRenderCount`・`GetSubLayerLocalMatrix3D`・`GetSubLayerOpacity`・`GetSubLayerXferMode`・`GetSubLayerBlur`・`GetSubLayerDepth`・`GetSubLayerRects`・`GetSubLayerXform3D`・`HasSubLayerXform`・`CanSubLayersRenderSeparately`・`SetSubLayersRenderSeparately` を公開している（SDK にも `AEGP_SubLayerIndex`、`AEGP_LayerFlag_SUBLAYERS_RENDER_SEPARATELY`、Canvas Suite の `AEGP_GetLayerAndSubLayerFromLayerContext` がある）。こちらのレイヤーに「サブレイヤーが N 個、それぞれこの変形」と答えさせれば、コピーは AE 自身の 3D レンダラーで本物として描かれる（隠しコンプも自前の合成も不要、正確さは作りから保証）。
- 静的解析で分かったこと（自動解析なしの部分的な逆コンパイル）: `BEE_AVLayer::HasSubLayerXform` は 0 を返し、`BEE_TextLayer` 版は 1 を返す（これが分かれ目とみられる）。`BEE_AVLayer::CanSubLayersRenderSeparately` は 1 を返す。そのほかの関数（`SubLayerRenderCount` など）は、自動解析なしでは関数の境界が取れず読めなかった（JUMPOUT）。
- ネット検索: この仕組みを外から使った公開例はない。既存の 3D プラグイン（Element 3D、Trapcode など）は自前のレンダラーで、AE のライト・影とは完全には混ざらない。Artisan（3D レンダラーの差し替え）は、レンダラーを丸ごと書く必要があり重い。
- **解析の道具**: 書き出しスクリプト `C:\Users\yakuruto\AppData\Local\Temp\claude\18403859059d0be6\997d682e-354f-4e6e-9fcb-96aa1b89dd6b\scratchpad\ida_dump.py`（idalib を Python から直接使い、公開関数と仮想関数表をファイルに書き出す。MCP の応答でトークンを使わないため。利用者の提案）。実行: `C:\Users\yakuruto\.local\idalib-mcp-venv\Scripts\python.exe ida_dump.py <dll> <出力先> <関数名の正規表現> [<vtable の正規表現>]`。出力: `scratchpad\ida\dump_sublayer\`（index.txt、fn_*.c、vt_*.txt）。スクラッチパッドなので消えることがある。
- 失敗の記録: idalib-mcp の全体の自動解析（23MB で約 15 分）を、終わった直後に止めてデータベースを消してしまった。

## （11/23 17:30）全体解析で見つけた隠し機能「レイヤーセット」（最有力）
- 全体解析済みの `.i64`: `spike/ida/BEE.dll.i64`（481MB、908 秒。消さない）。書き出し: `spike/ida/dump_sublayer`・`dump_layerset`・`dump_setnames`（index.txt から引く）。
- AV レイヤーのサブレイヤー数（`BEE_AVLayer::SubLayerRenderCount`）は、テキストとは別に **`ADBE Layer Sets` の各セットの `ADBE Set Copies`（コピー数）の積**で決まる。AE に、AV レイヤーをコピーとして並べる未公開の機能（シェイプのリピーターに似る）が入っている。
- 内部名: `ADBE Layer Sets` > `ADBE Layer Set` > {`ADBE Set Copies`, `ADBE Set Properties` > `ADBE Set Property`（参照するストリーム。`BEE_CmdSetLayerSetPropStreamRef`）> `ADBE Set Stops` > `ADBE Set Stop` > `ADBE Set Stop Pos`（Rel Pos）}。プロパティごとに「止まり点（相対位置と値）」を持ち、コピーの間を補間するとみられる。
- `BEE_AVLayer::CreateSet` はセットを足し、コピーを作り、レイヤーのスイッチ 0x80000 と 128 を立てる。0x80000 は `SubLayersRenderSeparately`（サブレイヤーを別々に描く）の条件のビット。つまりセットを作るだけで AE 自身がコピーを別々の板として描く見込み。
- セットのグループは、空なら UI で隠される（`UpdateStreamVisibility`）。保存形式の版 92 以上で書かれる。
- **試験 1（11/23 17:40、`spike/tools/run_probe.ps1 -Jsx scenes\probe_sets.jsx -Name sets`、結果は `spike/logs/probe_sets`）**: スクリプトから `L.property("ADBE Layer Sets").addProperty("ADBE Layer Set")` で**セットを足せる**（UI 名「セット」>「設定 1」>「コピー数」（既定 5）・「プロパティ」）。`collapseTransformation` は変わらない。ところが、**セットを足すとレイヤーが丸ごと描かれなくなる**（3D の白い平面・不透明度 50%、中央 128 → 0、全面が黒）。さらに **`コピー数.setValue(3)` で AE が止まる**（応答なし、強制終了）。仮説: スクリプトの経路では `CreateSet` が立てるスイッチ 0x80000・128 が立たない／プロパティ（止まり点）がないと描けない／機能フラグで止められている。
- **`AEGP_LayerH` は `BEE_Layer*` そのもの**（AEGPDriver の CreateNullLayer の実装が、出力の LayerH をそのまま `BEE_CmdSetLayerSwitch` に渡している。`spike/ida/AEGPDriver.aex.i64` に移した、`ida_importers.py` で確認）。
- **試験 2（17:46、`probe_sets2.jsx`＋プラグインの `ProbeSets`、環境変数 `DUPLIX_PROBE_SETS=create|switch`、`PF_Cmd_SEQUENCE_SETUP` で "Target" に対して実行。結果は `spike/logs/probe_create`）**: プラグインから `BEE_AVLayer::CreateSet` を呼べた（undo グループ内、例外なし）。**スイッチ 128 はコラップス**（`collapseTransformation` が true になる）。描画は元と同じ（中央 128、形も同じ）で、消えもしないが、コピーが重なった跡もない（5 枚なら約 251 のはず）。→ プロパティ（`ADBE Set Property`＋止まり点）がないとコピーに展開されないとみられる。`switch` モード（スクリプトで足した後にスイッチだけ立てる）は未実行。
- 次: コピーごとの変形の作り方を読む（`BEE_AVLayer::SubLayerRenderCount` の `*a3` が非空の経路 = `sub_180284090` がコピーの一覧（72 バイト/件）を作る）。プロパティの足し方は `BEE_LayerSet::AddProperty(BEE_StreamSpec const&)` と `BEE_CmdSetLayerSetPropStreamRef(BEE_Layer*, TDB_StreamIDPath const& set側, TDB_StreamIDPath const& 参照先)`、止まり点は `BEE_LayerSetStops::AddStop(BEE_Layer*, TDB_Stream*, double)`。UI の入口は AfterFXLib.dll（巨大、未解析）。
- 未確認: スクリプトから `addProperty("ADBE Layer Set")` できるか（→ できた）、コピーごとの変形がどう決まるか（`GetSubLayerLocalMatrix3D` は vtable +1800 と `sub_180284090` のキャッシュに委ねる）、ライト・影・被写界深度が効くか。

## （11/23 19:35〜）レイヤーセットの続き（試験 3〜）
- **コピーの変形の作り方（`BEE_LayerSet::GetSubLayerRects` の逆アセンブル）**: コピー数 N、セットのプロパティのうち参照先が位置・X/Y/Z 回転のものだけを使い、コピー i = `i × (止まり点 0 の値 − 今の値) / N` の平行移動と回転。等間隔の列のみ（ランダムも拡大縮小もなし）。複数セットは掛け合わせ。プロパティがないと一覧が空（試験 2 で何も起きなかった理由）。
- 一覧は `BEE_AVLayer::NewXformCache2D`（vtable +0x768）が vtable +0x748 `GetSubLayerRects` を呼んで作り、キャッシュの +96（M4、128B）/+120（M3、72B）/+144（矩形、32B）に入る。`SubLayerRenderCount`（+0x6D8）はキャッシュありなら M3 の件数、なしならコピー数の積。
- `SubLayersRenderSeparately`（+0x6C8）= フラグ 0x80000 かつ（AV 型で 3D ビット 0x400 / テキスト / シェイプ / 型 7）。
- `AEGP_StreamRefH` の中身: +120 がレイヤー、+80 が `TDB_StreamIDPath`。ここから `BEE_StreamSpec(layer, path)`（48 バイト、vptr あり）を作れる。`AddProperty` は止まり点を 1 つ作り、位置なら値を +(100,100,100) にする。
- **試験 3（prop モード、`spike/logs/probe_prop*`）**: `CreateSet`＋`AddProperty(位置)` に成功（UI 名「位置 1」「終了点」「停止 1」）。プラグインからレイヤーに聞くと、別々に描く=1、コピー数=5、行列は (0,0,0),(20,20,20),(40,40,40)…（**行ベクトル形式、4 行目が平行移動**）。**ところが描画は変わらない**（キャッシュを捨てても同じ）。
- **原因（途中）**: 描画は別のレイヤーオブジェクト（描画用の複製）に対して行われ、その複製ではフラグ 0x80000 が立っておらず、コピー数 0。→ 対象のインスタンスだけ vptr を差し替える方式は使えない（描画側に効かない）。
- 保存して開き直すと（セットは残る、sets n=1）、描画（`saveFrameToPng`）で AE が止まる。ただしこのときは共有の vtable にログ用の包みが入っていた（包みの戻り値の型の推測違いが原因の可能性あり）。
- Artisan 側: レイヤー一覧を作る `sub_1803855B0` が「別々に描く」ならコピー数ぶんのエントリを作る。クラシック 3D はここを通る見込み。
- 道具（`spike/ida/`）: `ida_fn.py`（関数とその呼び先、`vt:正規表現:枠数`、`asm:0xADDR`）、`ida_vcall.py`（`call/mov [reg+disp]` で仮想呼び出しの場所を探す）、`ida_callers.py`（呼び元を数段）、`ida_bytes.py`（定数を読む）。`BEE_exports.txt`・`BEE_imports.txt`・`AEGP_imports.txt` は dumpbin の出力。
- プラグイン（`DupliXSpike.cpp` の `ProbeSets`）: `DUPLIX_PROBE_SETS=prop`、`DUPLIX_PROBE_SW=0x400`（追加のスイッチ）、`DUPLIX_PROBE_VTLOG=1`（そのインスタンスだけ）／`g`（共有の vtable に包み）。シーンは `scenes/probe_sets2.jsx`（purge・保存・開き直し付き）。判定はスクラッチパッドの `bbox.py`（範囲と明るさの段）。
- ~~次: (1)〜(3)~~ → 実施済み（下記）。

### （11/23 20:45）試験 4・5 の結果
- **ロガーなしで開き直し → AE が落ちる**（止まるのではなく、自分で終了。ダンプは見つからず）。ENTER/LEAVE ログでは、描画スレッドの複製で `別々に描く → コピー数 → NewXformCache2D → GetSubLayerRects` と進み、**`GetSubLayerRects`（本来の `BEE_LayerSet` 経路）の中で落ちる**。→ 本来のレイヤーセット機能は、描画では使えない（壊れている／未完成）。
- 同じセッション内では、複製のセット数は 0（`sets=0`）。**SEQUENCE_SETUP からプラグインが BEE を直接いじった変更は、描画用の複製に同期されない**（スクリプトでの変更は届く。試験 1）。
- **試験 5「fill」（`DUPLIX_PROBE_SETS=fill`、`spike/logs/probe_fill*`）**: セットは使わず、**共有の vtable の 3 枠を差し替え**る（217 別々に描く→真、219 コピー数→5、233 `GetSubLayerRects`→元を呼ばずに矩形・M3・M4 を自分で 5 件詰める。メモリは ucrtbase の malloc、4096 以上は MSVC の 32 バイト整列の取り決め）。対象は `BEE_Layer::GetID()`（公開関数）で見分ける（描画用の複製でも同じ ID=17 だった）。行列は U.dll の `M_Matrix3/4::SetIdentity/Translate`。x を 60 ずつずらした 5 枚。
  - **落ちない**。描画スレッドが複製に対して、コピーごとに `GetSubLayerXform3D`(225)・`GetSubLayerLocalMatrix3D`(168) を 5 回、`Rasterize2DGraph`(235)・`GetBoundsFromCache`(229)・`GetSubLayerXform2D`(222/223) を呼ぶ（= AE がコピーを 5 枚の板として扱い始めている）。
  - **ところが絵ではレイヤーが消える（全面黒、f_after・f_reopen とも）**。試験 1（スクリプトでセットを足すと消えた）と同じ症状。
  - 不透明度（226）は常に 1.0、範囲（229）はこちらの M3 と矩形から計算 → この 2 つは原因ではない。
- **次の 1 手**: 消える原因を探す。候補: (a) コピーごとの絵（`Rasterize2DGraph` の結果、あるいは `BEE_RenderTextureForArtisan` のサブレイヤー版）が空、(b) 行列の座標系の違い（M4 の平行移動の単位・原点、2D 側 M3 との組み合わせ）、(c) `GetSubLayerXform2D` の 4 つの版が返す行列のどれかが未設定。まず平行移動 0（単位行列 5 枚）にして、重なって明るくなる（約 1−0.5^5）か消えるかを見る。消えるなら (a)、出るなら (b)。ログは `DUPLIX_PROBE_VTLOG=g` を併用（fill と同時に使える）。
- **利用者の仮説（有力）: ベクターとラスターの違い**。サブレイヤーの仕組みは文字単位 3D のテキスト（ベクター）用で、コピーごとに形から描き直せる。平面（ラスター）では「サブレイヤー i の絵」を作れず空になる → 消える。エフェクトが付くとラスター化（Flatten）されるので別々に描かない判定になる、とも一致する。反する点: レイヤーセットは一般の AV レイヤーに付いている（ただし本来の経路は落ちる＝作りかけの可能性）。**確かめ方**: (1) 平面で単位行列 5 枚 → 消えるなら「コピーの絵が空」が確定。(2) シェイプレイヤーを対象に同じ差し替え（シェイプは別の vtable なので、その表の 217/219/233 を差し替える）→ 描ければ仮説を支持。正しければ、「サブレイヤー i の絵」の要求にレイヤー全体の絵を返す箇所（`Rasterize2DGraph` 周辺）も差し替える必要がある。
### （11/23 22:10）試験 6: 仮説の確認 → **支持された**
- 環境変数: `DUPLIX_FILL_SPACING`（x の間隔、既定 60）、`DUPLIX_FILL_COL=1`（M4 の平行移動を列側へ）、`DUPLIX_FILL_M3ID=1`（M3 は単位行列のまま）、シーン側 `DUPLIX_TARGET_KIND=shape`（対象をシェイプの白い四角に）。ログは `spike/logs/probe_fill0`・`probe_fillshape`・`probe_shape0`・`probe_shapecol`・`probe_shapem3`。
- 平面（ラスター）・間隔 0 → **消える**。行列は無関係で、コピーの絵が空。
- シェイプ・間隔 0 → 元と同じ形で中央 248（50% の板 5 枚の理論値 ≈247）。**AE 自身の 3D 描画で、5 枚が別々の板として正しく合成された**。
- シェイプ・間隔 60（M3 と M4 の両方に平行移動）→ 遠近のついた板が 3 つ、台形にゆがみ、切れて見える（`probe_fillshape/f_after.png`）。
- 平行移動を M4 の列側へ → 1 枚分に見える（行側が AE 本来の形）。
- M3 は単位行列・M4 だけ平行移動 → 板が画面全体に広がる（射影がおかしい、`probe_shapem3/f_after.png`）。
- **分かったこと**: M3（2D）と M4（3D）の意味・座標系が分かっていない。テキストの本物の値を見るのが早い → `BEE_TextLayer::GetSubLayerRects`（vtable 233 のテキスト版）を逆コンパイルするか、文字単位 3D のテキストで差し替えずに 233 を包んでログに M3・M4・矩形を出し、文字ごとの値の作り方を真似る（テキストは BEE_TextLayer の別の vtable）。
### （11/23 夜）試験 7: **シェイプで任意の配置に成功**
- 規則（AV 版の `GetSubLayerXform2D` と試験から）: **コピー i の絵 = 元のレイヤーのうち M3(rect) の範囲の中身、その板を M4 で 3D に置く**（M4 は行ベクトル形式、4 行目が平行移動、レイヤー座標の単位）。テキストでは M3 が文字の位置、M4 が文字の 3D 変形。
- **M3 = 単位行列、M4 = 平行移動 → 正解**（`DUPLIX_FILL_M3ID=1`、`spike/logs/probe_shapem3b/f_after.png`）: 横 100・不透明度 50% の板 5 枚が 60 間隔に並び、重なり 40 の幅だけ 192、他は 128、全体の幅 302（理論 ≈299）。前の `probe_shapem3` が画面いっぱいに崩れたのは、前の実行の `DUPLIX_FILL_COL` が残っていたため（今はログに設定値を出す）。
- 環境変数は PowerShell の呼び出しをまたいで残ることがある。試験ごとに明示的に消すこと。
- テキストの文字ごとの M4 は TXT.dll の `TXT_GetCharacter3DTransformPlus` が作る（未解析、不要になった）。テキストの xform キャッシュは AV と別の構造（+96 等を読むと落ちる）。
### 試験 8: **ラスター（平面）でも成功**
- シェイプは `BEE_VectorLayer`。AV との違いで効くのは `Rasterize2DGraph`(235)。AV 版はキャッシュなしで `SubLayerRenderCount` を聞き、2 以上だとセット用の特別なノード `sub_180299BB0` で描く（セットのデータがないので空）。→ **キャッシュなし（shared_ptr が空）で聞かれたら元の答え（0）を返す**（`DUPLIX_FILL_NOCACHE_ORIG=1`）と、通常のノードでレイヤー全体の絵になり、平面でもシェイプと同じ正解の絵（`spike/logs/probe_solid2`、開き直しも同じ）。
- **今の「fill」の全条件**: 共有の AV vtable の 217→真、219→N（キャッシュありのときだけ）、233→矩形=元の矩形・M3=単位行列・M4=コピーの変形。`DUPLIX_PROBE_SETS=fill DUPLIX_FILL_M3ID=1 DUPLIX_FILL_NOCACHE_ORIG=1`。
### 試験 9: **対象にエフェクトが付いていても成功**
- エフェクトがあると、Artisan の判定 `sub_180385CD0`（BEE RVA 0x385CD0〜0x385DB0）が「AV 型（`IsLayerType(0)`）かつエフェクトあり」で真になり、コピーに分かれない（`probe_solidfx`、確認済み）。
- 対策: slot 79 `IsLayerType` を差し替え、**呼び元（`_ReturnAddress()`）がこの関数の範囲のときだけ、対象レイヤーに偽を返す**（`DUPLIX_FILL_FXBYPASS=1`）。→ 5 枚に分かれ、エフェクト（塗り：赤）も全コピーに入る（`spike/logs/probe_solidfx2/f_after.png`）。シーン側は `DUPLIX_EFFECT_ON_TARGET=1`。
- 注意: アドレスは今の AE 2025 の BEE.dll 固定。製品ではバイト列のパターン検索などで探す必要がある。
### 試験 10: **3D シーンで正解と画素単位で完全一致**（`spike/scenes/probe_3d.jsx`、`spike/logs/probe_3d`）
- シーン: クラシック 3D、床（X 回転 90）、交差する青い壁（Y 回転 90）、ライト（スポット、影なし）、カメラ。"probe" は Target 1 枚＋fill（固定の配置表 5 件、x/y/z ばらばら、`DUPLIX_FILL_TABLE=1`）、"ref" は本物の複製レイヤー 5 枚を同じ位置に。
- **差の最大 0（12 万画素すべて同じ）**。陰影・壁との交差・前後関係を含めて、AE 自身が描いたものと同一。
- 実行: `DUPLIX_FILL_TABLE=1 DUPLIX_FILL_FXBYPASS=1 DUPLIX_PROBE_SETS=fill DUPLIX_FILL_M3ID=1 DUPLIX_FILL_NOCACHE_ORIG=1`、`run_probe.ps1 -Jsx scenes\probe_3d.jsx -Name 3d`、比較はスクラッチパッドの `cmp.py`。
- **未解決**: スクリプトからライトの「影を落とす」を設定すると、見えないデスクトップの AE が止まる（`DUPLIX_3D_SHADOWS=1` で再現。ダイアログ待ちの可能性）。影の試験は別の手段で設定する必要がある。
- **試験 11: 回転も完全一致（差 0）**（`spike/logs/probe_3drot2`）。M4 = T(−アンカー)·Rz·Ry·Rx·T(アンカー＋オフセット)（行ベクトル、角度は度で、AE の Rotate X/Y/Z と同じ符号）。順序を X→Y→Z にすると 3 軸を組み合わせたコピーだけずれる（`probe_3drot`）。`DUPLIX_FILL_ROT=1 DUPLIX_FILL_ROTORDER=ZYX`。アンカーは矩形の中心（既定のアンカー）。
- **試験 12: 被写界深度（絞り 120）＋調整レイヤー（ガウスぼかし 4）＋モーションブラー（カメラを 1 秒で x+600）を全部入れても完全一致（差 0）**（`spike/logs/probe_3dx`、`DUPLIX_3D_EXTRA=dof,adj,mb`）。ただしコピー自身は動いていない（コピーの動きのブラーは、時間ごとに行列を変える段階で確かめる。hook は時間引数を今は無視）。
- **試験 13: 動くコピーのモーションブラーも一致（差の最大 1/255）**（`spike/logs/probe_3dmb3`、`DUPLIX_FILL_ANIM=1 DUPLIX_3D_TIME=0.5 DUPLIX_3D_EXTRA=mb`）。`GetSubLayerRects` の第 3 引数は時刻 `{int value; unsigned scale}`（レイヤー時間とみられる）で、AE はシャッター内の時刻（0.50104 s…）ごとに呼んでくる → 時間の関数として行列を返せばモーションブラーが正しく付く。
  - 注意: `probe_3d.jsx` のコンプの長さを 1/30 秒にしていた（`addComp` の引数の順を誤り）。2 秒に直した。0.5 秒で真っ白になったのはこのため。
- **影（未解決）**: ライトのオプション（「影を落とす」など、強度より後ろの項目）にスクリプトで触ると、見えないデスクトップの AE がモーダルのダイアログ（`#32770`、中身は AE 独自描画で読めない、入力欄 1 つ）で止まる。ライトの種類はスポット（4413）。プラグインからの `AEGP_SetStreamValue` も失敗（0）し、同様に止まる。CPU は増えない＝ダイアログ待ち。`WM_COMMAND IDOK` では進まない。→ 案: 利用者に影つきのシーンを手で作った .aep を用意してもらい、それを開いて試す。
- **試験 14: キャッシュ**（`spike/logs/probe_3dcache`、`DUPLIX_3D_CACHETEST=1 DUPLIX_FILL_FILE=<倍率のファイル>`）: hook が返す配置を外から変えただけでは、AE は古い絵を返す（`f_p2_nochange` = 倍率 1 の正解）。**対象レイヤーのエフェクトのパラメータ（スライダー）を変えると問い合わせ直して、新しい配置で正解と一致**（`f_p3_slider` = 倍率 0.5 の正解、差 0）。→ 製品では、配置をエフェクトのパラメータだけから決めれば、キャッシュは AE の仕組みのまま正しく働く。
- **試験 15: hook の中（描画スレッド）で、描画用の複製レイヤーからエフェクトのパラメータを AEGP で読める**（`spike/logs/probe_3dparam`、`DUPLIX_FILL_PARAM=1`）。`AEGP_EffectSuite4` でレイヤーのエフェクトを列挙→マッチ名で探す→`AEGP_GetNewEffectStreamByIndex`→`AEGP_GetNewStreamValue`（レイヤー時間、時刻は hook の第 3 引数）。SPBasicSuite は SEQUENCE_SETUP で保存したものを使う。止まりも落ちもなし。スライダー 0→倍率 1 の正解、5→倍率 0.5 の正解と差 0。→ **配置をエフェクトのパラメータだけから決める製品の形が成り立つ**（ID の対応表は、hook を入れる対象の判定にだけ使う。製品では「このエフェクトを持つレイヤーか」で判定できる）。
- **未確認（重要）**: ここまでの一致はすべて「エフェクトを付けた直後に SEQUENCE_SETUP で hook を入れ、同じ AE の中で `saveFrameToPng`」で得たもの。aerender（新しいプロセス）・MFR・読み込み直したプロジェクト・影・画像素材は未確認。
- **試験 16（11/24）: aerender（新しいプロセス、保存した .aep）で MFR オフ・オンとも完全一致（差 0）**。`spike/tools/run_3d_aerender.ps1 -Name 3daer`（シーン生成→`TARGET_ID` を jsx.log から取得→aerender→`r_probe`/`r_ref` の TIFF を比較）。hook は GLOBAL_SETUP で `GetProcAddress(BEE.dll, "??_7BEE_AVLayer@@6B@")` の表に入れる（`DUPLIX_FILL_GLOBAL=1 DUPLIX_FILL_LAYERID=<id>`）。slot 217 は 1 フレームで十数回、ID の判定は合計 0.04 ms。比較スクリプトは `DUPLIX_CMP_PY` で渡す。
- **試験 19（11/24）: アドレスの決め打ちをなくした版で、PNG 素材＋影のシーンが aerender の MFR オフ・オンとも完全一致**（`spike/logs/probe_3dshape`、`run_3d_aerender.ps1 -Name 3dshape [-SkipScene]`）。
  - 仮想関数表の枠は、公開関数（`?IsLayerType@BEE_AVLayer@@UEBAEH@Z` など 4 つ）のアドレスと表を照合して実行時に求める（`FindSlot`）。見つからなければ hook を入れない。
  - エフェクト判定のかわし方は、`IsLayerType(0)` の戻り先の命令の並びで判定する（`IsFlattenCheckSite`）: `test al,al; jnz; call BEE_BetaFeature_AreExpressionEffectsAlwaysAllowedEnabled; test; jz; mov rax,[rbx]; mov rcx,rbx; call [rax+d]; mov rdx,[rax]; mov rcx,rax; call [rdx+d]; test eax,eax; jnz; mov rax,[rbx]; mov rcx,rbx; call [rax+d]; test al,al; jnz; mov rcx,rbx; call (GetSelectedTrackMatte)`。前半だけだと**音声合成の判定 `sub_18043E350`（`HasAudio` から）にも合致してしまった**ので、後半まで照合する。合致は BEE+0x385CF9 だけになった。`DUPLIX_FILL_FXBYPASS=addr` で旧来の固定アドレス方式。
- **試験 22: 枚数と重さ**。1 回目（`perf.out`）は**無効**: シーンを使い回したため、N=1000・5000 では AE のディスクキャッシュから N=5 の絵が返り、hook は 1 度も呼ばれなかった（hook の配置はキャッシュの鍵に入らない。製品ではパラメータから決めるので問題ない）。2 回目の結果（`perf2.out`）: **N=7 → 72.7/48.0 s、N=1000 → 54.4/54.5 s（MFR オフ/オン、起動込み。1000 枚は N=7 と差がない）、N=5000 → 300 秒で打ち切り（オフ・オンとも）**。hook には N=5000 で問い合わせが来ている。3 回目の結果（`perf3.out`）: **N=2500 → 74.2 s（起動を除くと約 20 秒）**。`FillRects` は 1 回 67 ms（デバッグビルド）で全体のごく一部。→ 時間の大半は AE の 3D 描画側。4 回目の結果（`perf4.out`）: **N=5000 → 512 s（起動を除くと約 455 秒）**。2500 枚の約 20 秒から急増（枚数 2 倍で 20 倍超）。hook 側は `FillRects` 1 回 18 ms、slot 217 は 1 万回で ID 照合合計 5 ms → **時間はほぼすべて AE のクラシック 3D 描画側**（板どうしの交差の分割などで枚数に対して急に重くなるとみられる。同じ枚数の本物のレイヤーでの比較は未実施）。→ 設計の相談事項: 枚数の目安、重さの対策（交差しにくい配置、品質設定など）。2 回目の方法: N ごとにシーンを作り直し、Target に「スライダー制御 = N」を付けて鍵を変える（`DUPLIX_3D_NOREF=1`、`-Name 3dperf<N>`、N=7・1000・5000、出力はスクラッチパッドの `perf2.out`）。時間は AE の起動（約 55 秒）込みで、probe と ref の 2 フレーム分。
- **試験 23 の 1 回目: 失敗（差の最大 86）**。slot 226 `GetSubLayerOpacity` はクラシック 3D から 1 度も呼ばれない（`spike/logs/probe_3dopacity`）。
- **試験 23 の 2 回目の結果（11/24 05:53）: 成功。aerender の MFR オフ・オンとも正解との差 0**（`spike/logs/probe_3dopacity2`、出力はスクラッチパッド `a66d4780-…\scratchpad\opacity2.out`）。ログに `FILL artisan opacity sub=0..4 base=1.0 x 0.30/0.84/0.61/0.38/0.92` が出た（IAT の差し替えが効いた）。環境変数: `DUPLIX_FILL_TABLE=1 FILL_FXBYPASS=1 3D_SHADOWS=1 FILL_ROT=1 FILL_ROTORDER=ZYX FILL_NOCACHE_ORIG=1 PROBE_SETS=fill FILL_M3ID=1 FILL_OPACITY=1 3D_IMAGE=spike\assets\probe_img.png`、`DUPLIX_CMP_PY`=旧スクラッチパッド `1c09b12a-…\scratchpad\cmp.py`。→ コピーごとの不透明度は slot 226 ではなく `BEE_GetArtisanLayerOpacity`（AEGPDriver の IAT）で付ける。**次は「次の 1 手」の 4（設計の相談、`docs/decisions-2026-11-24.md`）。**
- （旧）**試験 23 の 2 回目（ビルド済み・未実行）**: AEGPDriver.aex の取り込み表（IAT）で `BEE_GetArtisanLayerOpacity` を `HookArtOpacity` に差し替え（`PatchImport`）、元の値 × コピーごとの値を返す。コピーの番号はトレースから取る: `v = *(char**)(trace+16)`、レイヤー = `*(void**)(v-48)`、サブレイヤー番号 = `*(int*)(v-40)`（`BEE_GetArtisanLayerFromTrace` の逆コンパイルで確認）。この関数は BEE 内からは直接呼ばれず、AEGPDriver の `sub_18001A150`（Artisan に層の不透明度を渡す AEGP の関数）から呼ばれる。実行: 下の 1 回目と同じ環境変数で `run_3d_aerender.ps1 -Name 3dopacity2`。ログの `FILL artisan opacity` が出るかと、正解との差を見る。
- **試験 23: コピーごとの不透明度**（1 回目の方法）。slot 226 `GetSubLayerOpacity`（`double (int i, shared_ptr<XformCache> const&)`、公開名で照合）を差し替え、i 番目のコピーに 0.3＋0.7×((i×37) mod 10)/9 を返す（`DUPLIX_FILL_OPACITY=1`）。ref の複製にも同じ不透明度。5 枚の表、PNG 素材、影あり。`run_3d_aerender.ps1 -Name 3dopacity`、出力はスクラッチパッドの `opacity.out`。
- 利用者向けの判断材料: `docs/decisions-2026-11-24.md`（方式の確定、回転の中心、散らす座標の基準、枚数と重さ、ばらつきの種類）。
- **試験 21 の結果: 成功**。saveFrameToPng・aerender（MFR オフ・オン）のすべてで正解（本物のレイヤー 100 枚）との差が最大 1/255（`spike/logs/probe_3drand100`）。大きい確保の経路も問題なし。slot 217 は 100 回で ID 照合の合計 0.01 ms。
- **試験 21: 乱数で 100 枚**。`DUPLIX_FILL_RANDOM=100`（プラグインの `RandomXform` と jsx が同じ Park–Miller 法（種 12345）で位置 ±150/±100/±300・回転 ±60° を作る）。PNG 素材、影なし。`run_3d_aerender.ps1 -Name 3drand100`、出力はスクラッチパッドの `3drand.out`。100 枚だと M4 の一覧が 12800 バイト → `VecReserve` の 4096 以上の整列確保の経路を初めて通る。
- **試験 20 の結果: 成功**。2 枚の対象（ID 53・54）が登録され、それぞれ自分のスライダー（0→倍率 1、5→0.5）を描画スレッドで読み、saveFrameToPng と aerender（MFR オフ・オン）のすべてで正解と完全一致（`spike/logs/probe_3dmulti`）。ID の照合は 10 回で合計 0.003 ms。
- **試験 20: 目印のエフェクトで対象を見分ける＋複数レイヤー**。
  - 1 回目: hook の判定 `IsMine` の中で AEGP のエフェクト列挙を呼んだら、シーン生成中（エフェクト適用の直後）に AE が止まった（UI スレッドの BEE の仮想関数の中から AEGP を呼ぶのは危険）。
  - 2 回目（今）: hook の中では**登録済みのレイヤー ID の一覧と照合するだけ**。一覧は SEQUENCE_SETUP でコンプのレイヤーを AEGP で調べて登録（試作）、aerender では `DUPLIX_FILL_LAYERIDS=53,54`（`run_3d_aerender.ps1` が jsx.log の全 `TARGET_ID` から渡す）。製品では自分のエフェクトが各コマンドで自分のレイヤー ID を登録する形になる。
  - `DUPLIX_FILL_BYEFFECT=1`、`DUPLIX_FILL_PARAM=1`（倍率をそのスライダーから）、シーン `DUPLIX_3D_MULTI=1`（probe に Target＝スライダー 0→倍率 1 と Target2＝スライダー 5→倍率 0.5、P0 から (80,40,120) ずらす。ref には両方のコピー）。影・PNG 素材も入れて、`run_3d_aerender.ps1 -Name 3dmulti` で saveFrameToPng と aerender（MFR オフ・オン）を比較。出力はスクラッチパッドの `3dmulti.out`。
- **次の 1 手（順に）**:
  1. ~~aerender で通す~~ → 済（試験 16）。旧メモ: hook を GLOBAL_SETUP で入れる（表は `GetProcAddress(BEE.dll, "??_7BEE_AVLayer@@6B@")` でインスタンスなしで取れる見込み）。対象の判定は、試作ではレイヤー ID を環境変数で渡す。`probe_3d.jsx` の最後でプロジェクトを保存し、レンダーキューに probe と ref を足す（`make_s1.jsx` の `addRQ` と同じく、TIFF のテンプレートを `RNDM_TIFF_A` として保存・適用）。aerender で描いて比べる。MFR のオン・オフの両方。slot 217 が全 AV レイヤーに何回呼ばれるかと、判定の時間も測る。
  2. ~~PNG の画像素材レイヤー~~ → **済（試験 17）: 半透明・模様つきの PNG（120×100、`spike/assets/probe_img.png`）で、saveFrameToPng と aerender の MFR オフ・オンのすべてで完全一致**（`spike/logs/probe_3dimg`、シーン側 `DUPLIX_3D_IMAGE=<png のパス>`）。
  3. ~~影~~ → **済（試験 18）: 影も完全一致（差 0）**（`spike/logs/probe_3dsh5`、`DUPLIX_3D_SHADOWS=1`）。床にコピーそれぞれの影が落ちる。止まっていた原因は、スクリプトで**ライトの「カラー」（`ADBE Light Color`、オプションの 6 番目）の値を読むと、見えないデスクトップの AE がダイアログ待ちになる**こと（内部名での検索もこれに触れて止まるとみられる）。回避: 7 番目以降を番号で回り、`matchName` だけを見て設定する。ライトの「影を落とす」の内部名は `ADBE Casts Shadows`（12 番目。`ADBE Light Casts Shadows` ではない）。旧メモ: ライトのオプションの項目ごとに、内部名を値を読む前にログへ出し、どの項目に触ると止まるかを特定する（1 回だけ）。回避できなければ、影つきの .aep を利用者に用意してもらう。
  4. 設計の相談に回すこと（設計書 5.4 節）: 回転の軸を矩形の中心にするかレイヤーの実際のアンカーにするか。対象レイヤー自身に回転・拡大縮小・親がある場合、配置をレイヤーの座標で行うかワールドの座標で行うか。
- その先: 製品の設計（hook を入れる時機と外す時機、複数インスタンス・複数レイヤー、MFR での同時呼び出し、AE の版ごとのアドレス探し、`IsLayerType` の戻りアドレス判定の範囲の探し方）、設計書の書き直しと利用者の承認。
- （旧メモ）ラスターのレイヤー（製品の本命）で使うには、「サブレイヤー i の絵」を作る処理（`Rasterize2DGraph`(235) 周辺）でレイヤー全体の絵を返させる必要がある。
- 未解決の製品上の問題（後で）: (1) 対象レイヤーにエフェクトが付いていると、Artisan の判定 `sub_180385CD0` でサブレイヤーとして描かれない（製品ではエフェクトを付けるので、ここも回避が必要）。(2) 差し替えたコピーの配置をキャッシュの鍵に反映する方法。(3) 共有 vtable の書き換えは全 AV レイヤーに効く（ID で絞るが、AE の版が変わると枠の位置が変わる）。

## 次の 1 手（11/23 17:55 更新。5 時間枠 90% で区切り、リセット 19:30）
0. **レイヤーセットでコピーを別々の変形で描かせる**。読む順: `BEE_AVLayer::SharedXformTimeCacheFactory` → `sub_1804B3BD0`（`sub_180284090` が呼ぶ。`BEE_LayerXformCache` を作り、+120/+128 の vector が 1 件 72 バイトのコピー一覧）→ そこから `BEE_LayerSet`／`ADBE Set Property`／止まり点がどう使われるか。書き出しは `spike/ida/ida_dump2.py BEE.dll.i64 <出力先> <正規表現>`（関数単体なら idalib で `ida_hexrays.decompile(ea)` をファイルへ）。
   - 次の試験: プラグインから Position を参照するセットのプロパティ（`AddProperty`／`BEE_CmdSetLayerSetPropStreamRef`）と止まり点 2 つ（`AddStop`）を作り、コピーが別々の位置に描かれるかを見る。中央の画素で判定（重なれば 1−0.5^n）。
   - 道具: `spike/tools/run_probe.ps1 -Jsx scenes\probe_sets2.jsx -Name <名前>`（`DUPLIX_PROBE_SETS`・`DUPLIX_SPIKE_LOG` を先に設定）。AE は毎回「閉じない → 90 秒後に強制終了」になるが、結果には影響なし。
   - コピー数の `setValue` はスクリプトから行うと AE が止まる（試験 1）。コピー数はプラグインから変えるか、止まる原因を調べる。
   - 利用者に見せる前に分かっていること: セットを作るとレイヤーのコラップスのスイッチが立ち、タイムラインに「セット」のグループが見える。
- （済）`BEE.dll` の全体解析: `spike/ida/BEE.dll.i64`（`.i64` は消さない・止めない）。
1. `BEE.dll` を全体の自動解析つきで開き直して `.i64` を保存する（約 15 分。MCP の `idb_open` か、`ida_dump.py` を `run_auto_analysis=True` に変えて実行）。その `.i64` から `ida_dump.py` で、サブレイヤー系の関数の本体、`BEE_AVLayer` の仮想関数表（`vt_*.txt`）、`HasSubLayerXform`・`SubLayerRenderCount` を呼ぶ側（クラシック 3D の描画処理）を書き出して読む。
2. 確かめること: (a) AV レイヤーでサブレイヤーが別々に描かれる条件（`HasSubLayerXform`・`SubLayersRenderSeparately` のフラグ・`SubLayerRenderCount`）、(b) 各サブレイヤーのテクスチャ範囲（`GetSubLayerRects`）と変形（`GetSubLayerLocalMatrix3D`）の決まり方、(c) `AEGP_LayerH` から `BEE_AVLayer*` を得る方法（AEGP の関数の実装を読む）、(d) 仮想関数表を書き換えて自分のレイヤーだけ答えを変える方法（ほかのレイヤーには元の関数を呼ぶ）。
3. 実験: 平面レイヤーに対してサブレイヤーを 2 個にし、別々の変形で 2 枚描かれるか → ライト・影・被写界深度・モーションブラーが効くか → 正解（本物の 3D レイヤー 2 枚）と RGB で一致するか。
4. 結果をもとに、(A) 隠しコンプ、(B) BEE への差し込み、(C) サブレイヤー方式 を利用者に比べて示す。
（以下は A' 時代のメモ）
（11/23 夕方: 実験はここで区切り、**設計の見直しに戻る**。設計書 5.4 節の未決事項を利用者と決める: エフェクトより上のレイヤー、2D レイヤー、平面でないもの、コピーの向きやばらつき、くり抜き方式の半透明での近似、対象より上の調整レイヤー・描画モード・レイヤースタイル、このエフェクト同士の前後。そのあと設計書の本文（4 節の他レイヤーの取得など）を今の方式に書き直して承認をもらい、writing-plans へ）
（以下は以前の一覧）
0. ~~`make_s1.jsx` の `addRQ` を直す。~~ → 完了。案: `setSettings` に渡す前に、設定から出力先の項目（"Output File Info" など）を消す。または `setSettings` のあとで `om.file` を設定し直して、`jsx.log` に `om.file` を出して確かめる。または、テンプレートを名前で 1 回だけ選び、以降は `rq.outputModule(1)` の `applyTemplate` を使わずに、レンダーキューの項目を `duplicate()` して `comp` を差し替えられないか試す。直したら `run_s1.ps1`（シーンの生成あり）で通す。
   - 合格の条件: マットの 4 種がすべて `render=aegp` で合格し、v0〜v3・t_mask・t_effect・t_collapse が崩れないこと。
   - まず `plugin.log` の `S6 matte:` で、マットのレイヤー（ビデオがオフ）が AEGP で画素を返すか（opaque px）を確かめる。空なら、そこから考え直す。
1. ~~はみ出すエフェクトの範囲を広げる方法~~ → 解決（S5c）。
2. ~~トラックマット（案 A）の実験~~ → 合格（7.4 節）: `AEGP_GetTrackMatteLayer` でマットのレイヤーを取り、それも AEGP で描画し、マットの種類（アルファ／反転アルファ／ルミナンス／反転ルミナンス）に従ってコンプ座標で掛け合わせる。t_matte で正解と形が一致するか。
3. ~~（任意）S5b~~ → 合格（7.5 節）: キャッシュの鍵に変換・カメラ・レイヤー一覧を混ぜる案（位置の取得も AEGP に移すかの判断）。
4. S3（スロットの自動割り当てとエクスプレッションの自動設定）: 位置と深度の取得にスロットを使い続けるかは S5・S5b の結果次第。絶対条件 1 のため、割り当てと設定はエフェクト自身が自動で行う必要がある。
5. ~~S4~~ → 合格（7.5 節）: 他レイヤーの移動・不透明度・マスク・エフェクト・プリコンプの中身の変更で再描画されるか（AEGP 方式ではマスクの変更も対象に戻す）。
6. 結果を設計書に反映し、5.4 節の未決事項を利用者と決めて、承認をもらう。

## 再開時の注意
- 実験コードの現状: `spike/DupliXSpike/DupliXSpike.cpp` は S5 用に AEGP の描画を足した版（`DUPLIX_S5_SITE`＝pre|smart、`DUPLIX_S5_SALT` で切り替え）。ビルドは `spike/tools/build.ps1`、実行は `spike/tools/run_s1.ps1`（`-SkipScene` でシーン生成を省略、`-Mfr ON|OFF`）。
- S4 の試験: `spike/tools/run_s4.ps1`（`s1.aep` が必要。出力は `spike/logs/s4`）。S5b の切り替えは環境変数 `DUPLIX_S5B`（0／compare／1）、対照は `DUPLIX_S4_NOGUID`・`DUPLIX_S5B_NOMIX`。
- 実行前に AfterFX / aerender が動いていないことを確かめる（利用者の AE を巻き込まない）。
- エフェクトが落ちると `Effect Crashed.txt` で次の起動時にダイアログが出る。`run_s1.ps1` は起動前に `Repair-AeCrashState` で防ぐ。「AfterFX did not close」と出たら、まずここを疑う。

<!-- 7章は実装なし，代わりにb-treeの説明がある -->
# B-Treeの説明

木構造はなぜデータベースで利用するのに都合が良いのか?
- 指定値での検索が高速である(対数時間)
- すでに見つけた値の挿入，削除が高速(リバランスにかかる時間が定数時間)
- 範囲のトラバースが高速

B-Treeは二分木とは違う(Bは開発者の名前から来ているが，Balancedを意味する)

以下にB-Treeの例を示す

<img src="https://cstack.github.io/db_tutorial/assets/images/B-tree.png">
<div align="center">
    fig1: B-Treeの例
</div>


二分木とは違い，B-Treeは各ノードに二つより多い部分木を持つことができる．  
各ノードは最大m個の部分木を持つことができる．（mは木のオーダーと呼ばれる）  
木の平衡を保つために，各ノードは少なくともm/2の部分木を持たなければならない

例外:

- 葉(末端)ノードは部分木を持たない
- ルートノードはm個未満の部分木を持てるが,最低でも2個
- ルートノードが葉ノードであるとき(全体でノードが一つ),部分木はない


fig1はSQLiteがインデックスを格納するために使うB-Treeである．
テーブルを格納するためには亜種であるB+ Treeが使われる.

|  | B-Tree | B+ Tree |
| - | - | - |
| 発音 | Bee Tree | Bee Plus Tree |
| 保存の用途 | インデックス | テーブル |
| 中間ノードがkeyを格納するか | yes | yes |
| 中間ノードがvalueを格納するか | yes | no |
| ノード毎の部分木 | 少ない | 多い |
| 中間ノード vs 葉ノード | 同じ構造 | 異なる構造 |

B-Treeを実装するまではB+ Treeについての説明を行うが，B-Treeまたはbtreeとして言及する

部分木を持つノードは中間ノードと呼ばれる. 中間ノードと葉ノードの違いは以下の通り.

| オーダがmの場合 | 中間ノード | 葉ノード |
| - | - | - |
| 格納対象 | キーと部分木のポインタ | キーとバリュー |
| キー数 | m-1が上限 | 収まるだけの数 |
| ポインタの数 | キー数+1 | なし |
| バリューの数 | なし | キー数と同じ |
| キーの目的 | ルーティング | バリューとのペアリング |
| バリューを保存するか | no | yes |

動作例を見ながら要素を挿入するときにB-Treeがどのように成長していくかを見ていこう.  
簡単のため，木のオーダー(m)は3とする.  
これは以下を意味する.

- 中間ノードは上限3までの部分木を持つ
- 中間ノードは上限2までのキーを持つ
- 中間ノードは少なくとも2の部分木を持つ
- 中間ノードは少なくとも1のキーを持つ

からのB-Treeは一つのノードをもつ.これはルートノードを表し，葉ノードとして扱われる(key/valueのペアが0個)

<img src="https://cstack.github.io/db_tutorial/assets/images/btree1.png">
<div align="center">
    fig2: 空のB-Tree
</div>

key/valueのペアを挿入すると，ソートされた順序で葉ノードに格納される

<img src="https://cstack.github.io/db_tutorial/assets/images/btree2.png">
<div align="center">
    fig3: ノードが一つのB-Tree
</div>

葉ノードのkey/valueペアの容量を2であるとする．  
ペアをさらに追加するとき, 葉ノードを分割し，各ノードに配置する必要がある.  
ここでできた二つの葉ノードは中間ノードになるノードの部分木となる(この中間ノードはルートノードである).

<img src="https://cstack.github.io/db_tutorial/assets/images/btree3.png">
<div align="center">
    fig4: level2のB-Tree(level=depth+1)
</div>

今，中間ノードは1つのキーと2つの部分木へのポインタを持っている.
5以下のキーを検索する場合，左の部分木を探索する．5より大きいキーを検索する場合，右の部分木を探索する.

キーが2のペアを挿入するとしよう.  
はじめに，格納できるノードがあるかを調べ，左の葉ノードに到達する．  
しかし，葉ノードの容量はいっぱいなので葉ノードを分割し，親ノードに新しいエントリを追加する.

<img src="https://cstack.github.io/db_tutorial/assets/images/btree4.png">
<div align="center">
    fig5: ノード4つのB-Tree
</div>

次に18と21のキーを追加する.  
またノードを分割する必要がある．しかし，親ノードはすでに3つの部分木を持っており，key/pointerのペアを格納する余地はない.

<img src="https://cstack.github.io/db_tutorial/assets/images/btree5.png">
<div align="center">
    fig6: 中間ノードにエントリを作成できない
</div>

解決法はルートノードを二つの中間ノードに分割することである.  
これで親ノードに新しいエントリが作成できる

<img src="https://cstack.github.io/db_tutorial/assets/images/btree6.png">
<div align="center">
    fig7: level3のB-Tree
</div>


木の深さはルートノードの分割が行われた時のみ増加する．全ての葉ノードは同じ深さにあり，key/valueのペア数はほぼ同じ値となる.  
これにより平衡を保ち，高速な検索が可能になる.

削除については挿入の実装後に言及する.

各ノードは一つのページと一致するルートノードはpage0に存在するし，部分木のポインタは単に個ノードを含むページ番号になる.

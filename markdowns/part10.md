# part10 - Splitting a Leaf Node

今の実装では，ノードが一つしかない木になっています．
これを修正するために，葉ノードを二つに分割するコードが必要です．  
その後，分割した葉ノードの親となる中間ノードを作る必要があります.

この章のゴールは, 以下のような構造から

<div align="center">
    <img src="https://cstack.github.io/db_tutorial/assets/images/btree2.png">
</div>
<div align="center">
    fig1: ノードが一つのbtree
</div>

以下のような構造に変更することです

<div align="center">
    <img src="https://cstack.github.io/db_tutorial/assets/images/btree3.png">
</div>
<div align="center">
    fig2: level2 のbtree
</div>


まずはじめに，Full Leaf Nodeエラーのハンドリングを消すところから始めます．

```c
 void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value) {
   void* node = get_page(cursor->table->pager, cursor->page_num);
 
   uint32_t num_cells = *leaf_node_num_cells(node);
   if (num_cells >= LEAF_NODE_MAX_CELLS) {
     // Node full
-    printf("Need to implement splitting a leaf node.\n");
-    exit(EXIT_FAILURE);
+    leaf_node_split_and_insert(cursor, key, value);
+    return;
   }
```

```c
ExecuteResult execute_insert(Statement* statement, Table* table) {
   void* node = get_page(table->pager, table->root_page_num);
   uint32_t num_cells = (*leaf_node_num_cells(node));
-  if (num_cells >= LEAF_NODE_MAX_CELLS) {
-    return EXECUTE_TABLE_FULL;
-  }
 
   Row* row_to_insert = &(statement->row_to_insert);
   uint32_t key_to_insert = row_to_insert->id;
```

## Splitting Algorithm
簡単なパートはおわりました．ここにやるべきことの説明が書いてあります.
[SQLite Datavase System: Design and Implementation](https://play.google.com/store/books/details/Sibsankar_Haldar_SQLite_Database_System_Design_and?id=9Z6IQQnX1JEC&hl=en)

> If there is no space on the leaf node, we would split the existing entries residing there and the new one (being inserted) into two equal halves: lower and upper halves. (Keys on the upper half are strictly greater than those on the lower half.) We allocate a new leaf node, and move the upper half into the new node.

> 葉ノードにスペースがない場合，すでにあるエントリと新しい（挿入される）エントリに等しく半分に分割します．（上半分のキーは下半分のキーよりも厳密に大きいです）  
> 新しい葉ノードを作成して，上半分を新しいノードに移動させます.

古いノードの操作と新しいノード作成を行います.

```c
+void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value) {
+  /*
+  Create a new node and move half the cells over.
+  Insert the new value in one of the two nodes.
+  Update parent or create a new parent.
+  */
+
+  void* old_node = get_page(cursor->table->pager, cursor->page_num);
+  uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
+  void* new_node = get_page(cursor->table->pager, new_page_num);
+  initialize_leaf_node(new_node);
```

次に，全てのcellを適切な場所(上半分は新しいノード，下半分は古いノード)に移動させます．

```c
+  /*
+  All existing keys plus new key should be divided
+  evenly between old (left) and new (right) nodes.
+  Starting from the right, move each key to correct position.
+  */
+  for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
+    void* destination_node;
+    if (i >= LEAF_NODE_LEFT_SPLIT_COUNT) {
+      destination_node = new_node;
+    } else {
+      destination_node = old_node;
+    }
+    uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
+    void* destination = leaf_node_cell(destination_node, index_within_node);
+
+    if (i == cursor->cell_num) {
+      serialize_row(value, destination);
+    } else if (i > cursor->cell_num) {
+      memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
+    } else {
+      memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
+    }
+  }
```

それぞれのノードのヘッダの総cell数を更新します.

```c
+  /* Update cell count on both leaf nodes */
+  *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
+  *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;
```

そして，親ノードを更新する必要があります．  
分割元のノードがルートノードの場合，親を持ちません．
この場合，新しい親として振る舞うルートノードを作成します．  
とりあえず，他の分岐をスタブ化しておきます.

```c
+  if (is_node_root(old_node)) {
+    return create_new_root(cursor->table, new_page_num);
+  } else {
+    printf("Need to implement updating parent after split\n");
+    exit(EXIT_FAILURE);
+  }
+}
```

## Allocationg New Pages
いくつかの関数と定数を定義しましょう.  
新しい葉ノードを作成したら、それを`get_unused_pa​​ge_num()`によって決定されたページに配置します.

```c
+/*
+Until we start recycling free pages, new pages will always
+go onto the end of the database file
+*/
+uint32_t get_unused_page_num(Pager* pager) { return pager->num_pages; }
```

今のところ，データベースはNページあると想定しています．  
ページナンバーが0からN-1のものはメモリに割り当てられています．
故に，新しいページは常にNを割り当てることができます.  
削除の実装の後には，いくつかのページは空になり，それらのページ番号は使われなくなります．
より効率的にするために，これら空のページを再割り当てすることができます．

## Leaf Node Sizes
木を平衡に保つために，二つの新しいノードに等しく分配します．
葉ノードがN個のcellを持っている場合，分割でN+1個のcellを二つのノードに分配します(Nは元のノードが持っているcell, 1は新しく挿入されるcell).  
N+1が奇数の場合，左のノードを一つ多くします(特に意味はないです.)

```c
+const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
+const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT =
+    (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;
```

## Creating a New Root
ここにルートノード作成の処理の説明があります((SQLite Database System)[https://play.google.com/store/books/details/Sibsankar_Haldar_SQLite_Database_System_Design_and?id=9Z6IQQnX1JEC&hl=en])

> Let N be the root node. First allocate two nodes, say L and R. Move lower half of N into L and the upper half into R. Now N is empty. Add 〈L, K,R〉 in N, where K is the max key in L. Page N remains the root. Note that the depth of the tree has increased by one, but the new tree remains height balanced without violating any B+-tree property.

> Nをルートノードとします.最初に二つのノードの割り当てを行い（LとRと言います），Nのうち下半分をLへ，上半分をRへ移動させます．いま，Nは空っぽです．Nに<L, K, R>を入れます(ここでKはLのキーの内，最大のものです)．PageNはルートノードのままです．ここで注意するのは木の深さは一つ増えましたが，新しい木はB+木のプロパティに違反することなく高さのバランスが保たれています．

この点について，すでに右側の子を割り当てて上半分を移動しました．この関数は右側の子を入力として受け取り，左側の子を格納する新しいページを割り当てます.

```c
+void create_new_root(Table* table, uint32_t right_child_page_num) {
+  /*
+  Handle splitting the root.
+  Old root copied to new page, becomes left child.
+  Address of right child passed in.
+  Re-initialize root page to contain the new root node.
+  New root node points to two children.
+  */
+
+  void* root = get_page(table->pager, table->root_page_num);
+  void* right_child = get_page(table->pager, right_child_page_num);
+  uint32_t left_child_page_num = get_unused_page_num(table->pager);
+  void* left_child = get_page(table->pager, left_child_page_num);
```

古いノードは左側の子にコピーされるので，ルートページを再利用できます．

```c
+  /* Left child has data copied from old root */
+  memcpy(left_child, root, PAGE_SIZE);
+  set_node_root(left_child, false);
```

最後に，ルートページを二つの子を持つ中間ノードとして初期化します．

```c
+  /* Root node is a new internal node with one key and two children */
+  initialize_internal_node(root);
+  set_node_root(root, true);
+  *internal_node_num_keys(root) = 1;
+  *internal_node_child(root, 0) = left_child_page_num;
+  uint32_t left_child_max_key = get_node_max_key(left_child);
+  *internal_node_key(root, 0) = left_child_max_key;
+  *internal_node_right_child(root) = right_child_page_num;
+}
```

## Internal Node Format

中間ノードを作成したので，そのレイアウトを定義する必要があります.  
共通ヘッダからはじまり，次にノードが持つキー数になります．
その次に最も右のこのページ番号になります．  
中間ノードは常にキーよりも一つ多い子へのポインタを持ちます  
その追加の子へのポインタはヘッダに格納されます.

```c
+/*
+ * Internal Node Header Layout
+ */
+const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
+const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
+const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
+const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET =
+    INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;
+const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
+                                           INTERNAL_NODE_NUM_KEYS_SIZE +
+                                           INTERNAL_NODE_RIGHT_CHILD_SIZE;
```

ボディはcell(キーと子へのポインタのペア)の配列を持ちます.  
中間ノードのcellのキーは左の子のキーの最大値となります.

```c
+/*
+ * Internal Node Body Layout
+ */
+const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
+const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
+const uint32_t INTERNAL_NODE_CELL_SIZE =
+    INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;
```

これらの定数を元に中間ノードのレイアウトは以下のようになります.

<div align="center">
    <img src="https://cstack.github.io/db_tutorial/assets/images/internal-node-format.png">
</div>
<div align="center">
    fig3: 中間ノードのレイアウト
</div>

重要な分岐要素があります．key/子へのポインタのペアはとても小さく，各中間ノードに510このキーと511個の子へのポインタを格納することができます.  
これは特定のキーを見つけるために木の層をたくさんたどる必要がないことを意味します！

| 中間ノードの層数 | 最大葉ノード数 | 葉ノードの総サイズ |
| - | - | - |
| 0 | 511^0 = 1 | 4KB |
| 1 | 511^1 = 512 | ~2MB |
| 2 | 511^2 = 261,121 | ~1GB |
| 3 | 511^3 = 133,432,831 | ~550GB |

実際のところ,ヘッダのオーバヘッド，キー，無駄なスペースがあるために，葉ノードごとに4KBのデータ全体を格納することはできません．  
しかし，ディスクから4ページを読み込むだけで，500GBのデータのようなデータの検索が行えます. これがB-Treeがデータベースにおいて有用なデータ構造である理由です.

以下は中間ノードの読み込みと書き込みを行うメソッドです.

```c
+uint32_t* internal_node_num_keys(void* node) {
+  return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
+}
+
+uint32_t* internal_node_right_child(void* node) {
+  return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
+}
+
+uint32_t* internal_node_cell(void* node, uint32_t cell_num) {
+  return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
+}
+
+uint32_t* internal_node_child(void* node, uint32_t child_num) {
+  uint32_t num_keys = *internal_node_num_keys(node);
+  if (child_num > num_keys) {
+    printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
+    exit(EXIT_FAILURE);
+  } else if (child_num == num_keys) {
+    return internal_node_right_child(node);
+  } else {
+    return internal_node_cell(node, child_num);
+  }
+}
+
+uint32_t* internal_node_key(void* node, uint32_t key_num) {
+  return internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
+}
```

**!Todo:** 正しい翻訳  
中間ノードにとって，最大値のキーは常に最も右にあるキーです  
また，葉ノードにとって最大値のキーはcellの最大indexの位置にあります.

```c
+uint32_t get_node_max_key(void* node) {
+  switch (get_node_type(node)) {
+    case NODE_INTERNAL:
+      return *internal_node_key(node, *internal_node_num_keys(node) - 1);
+    case NODE_LEAF:
+      return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
+  }
+}
```

## Keeping Track of the Root

最後に，共通ヘッダの`is_root`フィールドを使います．  
葉ノードをどのように分割するかを決めたことを思い出してください．

```c
  if (is_node_root(old_node)) {
    return create_new_root(cursor->table, new_page_num);
  } else {
    printf("Need to implement updating parent after split\n");
    exit(EXIT_FAILURE);
  }
}
```

`is_root`のセッタ，ゲッタを定義します

```c
+bool is_node_root(void* node) {
+  uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
+  return (bool)value;
+}
+
+void set_node_root(void* node, bool is_root) {
+  uint8_t value = is_root;
+  *((uint8_t*)(node + IS_ROOT_OFFSET)) = value;
+}
```

葉ノード，中間ノードの両方の`is_root`をデフォルトをfalseとして初期化します．

```c
 void initialize_leaf_node(void* node) {
   set_node_type(node, NODE_LEAF);
+  set_node_root(node, false);
   *leaf_node_num_cells(node) = 0;
 }

+void initialize_internal_node(void* node) {
+  set_node_type(node, NODE_INTERNAL);
+  set_node_root(node, false);
+  *internal_node_num_keys(node) = 0;
+}
```

テーブルの最初のノードを作成する時は`is_root`をtrueにセットします．

```c
     // New database file. Initialize page 0 as leaf node.
     void* root_node = get_page(pager, 0);
     initialize_leaf_node(root_node);
+    set_node_root(root_node, true);
   }
 
   return table;
```

## Printing the Tree

データベースの状態を可視化するには，`.btree`メタコマンドを多層の木をプリントできるように更新する必要があります．

`print_leaf_node()`関数を置き換えます.

```c
-void print_leaf_node(void* node) {
-  uint32_t num_cells = *leaf_node_num_cells(node);
-  printf("leaf (size %d)\n", num_cells);
-  for (uint32_t i = 0; i < num_cells; i++) {
-    uint32_t key = *leaf_node_key(node, i);
-    printf("  - %d : %d\n", i, key);
-  }
-}
```

任意のノードに対して，ノードのキーとその子を出力する再帰関数を定義します.
引数にインデンテーションレベルをとり，再帰の呼び出しごとにネストが深くなるようにします．  
インデントのためのヘルパー関数も定義します.

```c
+void indent(uint32_t level) {
+  for (uint32_t i = 0; i < level; i++) {
+    printf("  ");
+  }
+}
+
+void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level) {
+  void* node = get_page(pager, page_num);
+  uint32_t num_keys, child;
+
+  switch (get_node_type(node)) {
+    case (NODE_LEAF):
+      num_keys = *leaf_node_num_cells(node);
+      indent(indentation_level);
+      printf("- leaf (size %d)\n", num_keys);
+      for (uint32_t i = 0; i < num_keys; i++) {
+        indent(indentation_level + 1);
+        printf("- %d\n", *leaf_node_key(node, i));
+      }
+      break;
+    case (NODE_INTERNAL):
+      num_keys = *internal_node_num_keys(node);
+      indent(indentation_level);
+      printf("- internal (size %d)\n", num_keys);
+      for (uint32_t i = 0; i < num_keys; i++) {
+        child = *internal_node_child(node, i);
+        print_tree(pager, child, indentation_level + 1);
+
+        indent(indentation_level + 1);
+        printf("- key %d\n", *internal_node_key(node, i));
+      }
+      child = *internal_node_right_child(node);
+      print_tree(pager, child, indentation_level + 1);
+      break;
+  }
+}
```

また，プリント関数の呼び出しを更新します．
インデンテーションレベルを0で呼び出すようにします.

```c
   } else if (strcmp(input_buffer->buffer, ".btree") == 0) {
     printf("Tree:\n");
-    print_leaf_node(get_page(table->pager, 0));
+    print_tree(table->pager, 0, 0);
     return META_COMMAND_SUCCESS;
```

新しいプリントのテストケースです.

```ruby
+  it 'allows printing out the structure of a 3-leaf-node btree' do
+    script = (1..14).map do |i|
+      "insert #{i} user#{i} person#{i}@example.com"
+    end
+    script << ".btree"
+    script << "insert 15 user15 person15@example.com"
+    script << ".exit"
+    result = run_script(script)
+
+    expect(result[14...(result.length)]).to match_array([
+      "db > Tree:",
+      "- internal (size 1)",
+      "  - leaf (size 7)",
+      "    - 1",
+      "    - 2",
+      "    - 3",
+      "    - 4",
+      "    - 5",
+      "    - 6",
+      "    - 7",
+      "  - key 7",
+      "  - leaf (size 7)",
+      "    - 8",
+      "    - 9",
+      "    - 10",
+      "    - 11",
+      "    - 12",
+      "    - 13",
+      "    - 14",
+      "db > Need to implement searching an internal node",
+    ])
+  end
```

新しいフォーマットは簡略化されているので，すでにある`.btree`のテストを更新する必要があります

```ruby
       "db > Executed.",
       "db > Executed.",
       "db > Tree:",
-      "leaf (size 3)",
-      "  - 0 : 1",
-      "  - 1 : 2",
-      "  - 2 : 3",
+      "- leaf (size 3)",
+      "  - 1",
+      "  - 2",
+      "  - 3",
       "db > "
     ])
   end
```

新しい`.btree`メタコマンドのテストの出力は以下のようになっています．

```
Tree:
- internal (size 1)
  - leaf (size 7)
    - 1
    - 2
    - 3
    - 4
    - 5
    - 6
    - 7
  - key 7
  - leaf (size 7)
    - 8
    - 9
    - 10
    - 11
    - 12
    - 13
    - 14
```

インデントが最も浅いものは，ルートノード（中間ノード）が表示されます．
キーが1つなのでサイズ1と表示されます．インデントを一つ下げると，葉ノード，キー，葉ノードが表示されます.  
ルートノード(7)のキーは最初のリーフノードの最大キーです．7より大きい全てのきーは2番目の葉ノードにあります.

## A Major Problem
新しいノードを追加しようとするとどうなるでしょう？

```
db > insert 15 user15 person15@example.com
Need to implement searching an internal node
```

おっと！ 誰かTODOメッセージ書いた？

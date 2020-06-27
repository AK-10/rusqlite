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

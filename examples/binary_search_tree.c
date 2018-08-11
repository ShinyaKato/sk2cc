typedef struct node {
  int empty, value;
  struct node *left, *right;
} Node;

Node *node_new() {
  Node *node = malloc(24);
  node->empty = 1;
  return node;
}

int print(Node *node, int depth) {
  if (!node->left->empty) print(node->left, depth + 1);
  for (int i = 0; i < depth; i++) printf("  ");
  printf("%d\n", node->value);
  if (!node->right->empty) print(node->right, depth + 1);

  return 0;
}

int insert(Node *node, int value) {
  while (!node->empty) {
    node = value < node->value ? node->left : node->right;
  }

  node->empty = 0;
  node->value = value;
  node->left = node_new();
  node->right = node_new();

  return 0;
}

int main() {
  Node *root = node_new();

  for (int i = 0; i < 20; i++) {
    int value = rand() % 100 + 1;
    insert(root, value);
  }

  print(root, 0);

  return 0;
}

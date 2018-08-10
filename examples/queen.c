int A[10][10];

int dfs(int row, int n) {
  if (row == n) return 1;

  int count = 0;
  for (int col = 0; col < n; col++) {
    int ok = 1;
    for (int i = 1; i < n; i++) {
      if (row - i >= 0 && col - i >= 0) {
        ok = ok && A[row - i][col - i] == 0;
      }
      if (row - i >= 0) {
        ok = ok  && A[row - i][col] == 0;
      }
      if (row - i >= 0 && col + i < n) {
        ok = ok  && A[row - i][col + i] == 0;
      }
    }
    if (ok) {
      A[row][col] = 1;
      count += dfs(row + 1, n);
      A[row][col] = 0;
    }
  }
  return count;
}

int main() {
  for (int i = 1; i <= 10; i++) {
    printf("%d queen: %d\n", i, dfs(0, i));
  }

  return 0;
}

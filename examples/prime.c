int main() {
  int prime[1000000];

  for (int i = 0; i < 1000000; i++) {
    prime[i] = 1;
  }
  prime[0] = 0;
  prime[1] = 0;

  for (int i = 0; i < 1000000; i++) {
    if (prime[i]) {
      for (int j = i * 2; j < 1000000; j += i) {
        prime[j] = 0;
      }
    }
  }

  int count = 0;
  for (int i = 0; i < 1000000; i++) {
    if (prime[i]) count++;
  }
  printf("prime between [0, 1000000): %d\n", count);

  return 0;
}

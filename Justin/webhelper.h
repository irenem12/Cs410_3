

int getIntSize(int string, int base){
  int count = 1;
  while(string >= base){
      string /= base;
      count++;
  }
  return count;
}

void intToString(int input, int base, int size, char* string){

  //printf("Size: %d\n", size);
  int i = size - 1;

  for(; i >=0; i--){
    if(base > 10){
      if(input % base >= 10){
        string[i] = (input % base) % 10 + 'A';
      }else{
        string[i] = (input % base) + '0';
      }
    }else{
      string[i] = (char) (input % base + '0');
    }
    input /= base;
  }
}

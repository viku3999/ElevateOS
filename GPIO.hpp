#define GPIO_LOW 0
#define GPIO_HIGH 1
#define GPIO_OUT 3
#define GPIO_IN 4

int GPIO_Init(int GPIO_Pin, int Direction);

int GPIO_Set_Value(int GPIO_Pin, int Value);

int GPIO_Get_Value(int GPIO_Pin, int *Value);
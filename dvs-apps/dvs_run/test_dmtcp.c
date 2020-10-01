#include <stdio.h>
#include <unistd.h>

int main() {
    	int aux = 1;
    	while(1) {
            	printf("%d\n", aux++);
            	sleep(1);
    	}
}

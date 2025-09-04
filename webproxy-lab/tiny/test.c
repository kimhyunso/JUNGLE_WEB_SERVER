#include<stdio.h>
#include<string.h>

int main() {

    char* uri = "localhost:8000/cgi-bin/adder?num1=1000&num2=2000";
    // printf("%s\n", strstr(uri, "cgi-bin"));
    // printf("%d\n", !strstr(uri, "cgi-bin"));
    if (strstr(uri, "cgi-bin")) {
        printf("include cgi-bin");
    } else {
        printf("exclude cgi-bin");
    }


    return 0;
}
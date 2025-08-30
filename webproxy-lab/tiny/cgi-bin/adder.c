/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

// int main(void)
// {
//   char *buf, *p;
//   char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
//   int n1 = 0, n2 = 0;

//   /* Extract the two arguments */
//   if ((buf = getenv("QUERY_STRING")) != NULL)
//   {
//     p = strchr(buf, '&');
//     *p = '\0';
//     strcpy(arg1, buf);
//     strcpy(arg2, p + 1);
//     n1 = atoi(strchr(arg1, '=') + 1);
//     n2 = atoi(strchr(arg2, '=') + 1);
//   }

//   /* Make the response body */
//   sprintf(content, "QUERY_STRING=%s\r\n<p>", buf);
//   sprintf(content + strlen(content), "Welcome to add.com: ");
//   sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");
//   sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>",
//           n1, n2, n1 + n2);
//   sprintf(content + strlen(content), "Thanks for visiting!\r\n");

//   /* Generate the HTTP response */
//   printf("Content-type: text/html\r\n");
//   printf("Content-length: %d\r\n", (int)strlen(content));
//   printf("\r\n");
//   printf("%s", content);
//   fflush(stdout);

//   exit(0);
// }
// /* $end adder */



int main(void) {
	char* buf, *p;
	char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
	int n1=0, n2=0;

	if ((buf = getenv("QUERY_STRING")) != NULL) {
		p = strchr(buf, '&'); // buf 안에서 '&' 문자 찾음
		*p = '\0';
		strcpy(arg1, buf);
		strcpy(arg2, p+1);
		n1 = atoi(arg1); // 문자열 to INT
		n2 = atoi(arg2);
	}

	// make response body
	sprintf(content, "QUERY_STRING=%s", buf);
	sprintf(content, "Welcome to add.com: ");
	sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
	sprintf(content, "%sTHE answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
	sprintf(content, "%sThanks for visiting!\r\n", content);

	// generate HTTP response
	printf("Connection: close\r\n");
	printf("Content-length: %d\r\n", (int)strlen(content));
	printf("Content-type: text/html\r\n\r\n");
	printf("%s", content);
	fflush(stdout);

	exit(0);
}





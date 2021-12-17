/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"
/* CGI: Common gateway interface

adder: cgi 프로그램이라고 언급 => 이게 CGI 표준 룰을 따르기 떄문.


*/
/* Examples of CGI environment variables

QUERY_STRING: Program arguments 
SERVER_PORT: Port that the parent is listening on
REQUEST_METHOD: GET or POST
REMOTE_HOST: Domain name of client
REMOTE_ADDR: Dotted-decimal IP address of client
CONTENT_TYPE: POST only: MIME type of the request body
CONTENT_LENGTH: POST only: Size in bytes of the request body
*/

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, "&");
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcone to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s, content");
  fflush(stdout);

  exit(0);
}
/* $end adder */

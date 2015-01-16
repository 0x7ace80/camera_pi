#include<stdio.h>
#include<errno.h>
#include<string.h>

int sendmail(const char *to, const char *from, const char *subject, const char *message)
{
    int retval = -1;
    FILE *mailpipe = popen("/usr/sbin/sendmail -t", "w");
    if (mailpipe != NULL)
    {
        fprintf(mailpipe, "To: %s\n", to);
        fprintf(mailpipe, "From: %s\n", from);
        fprintf(mailpipe, "Subject: %s\n\n", subject);
        fwrite(message, 1, strlen(message), mailpipe);
        fwrite(".\n", 1, 2, mailpipe);
        pclose(mailpipe);
        retval = 0;
     }
     else
     {
         perror("Failed to invoke sendmail");
     }
     return retval;
}
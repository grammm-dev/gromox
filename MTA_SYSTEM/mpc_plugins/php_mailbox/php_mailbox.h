#ifndef _H_PHP_MAILBOX_
#define _H_PHP_MAILBOX_

void php_mailbox_init(const char *propname_path, int speed_limitation);

int php_mailbox_run();

int php_mailbox_stop();

void php_mailbox_free();

#endif /* _H_PHP_MAILBOX_ */

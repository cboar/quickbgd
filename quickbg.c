#include <stdio.h>
#include <mqueue.h>


int main(int argc, char **argv) {
	if(argc < 2){
		return 1;
	}

    mqd_t mq = mq_open("/quickbgd", O_WRONLY);
	if(mq == (mqd_t) -1){
		perror("Error opening message queue");
		return 1;
	}

	char buffer[] = { argv[1][0] };
	mq_send(mq, buffer, 1, 0);
	mq_close(mq);

    return 0;
}

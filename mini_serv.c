#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>

int	clients = 0, max_fd = 0;
int	ids[65536];
char	*msg[65536];

fd_set	rfds, wfds, fds;
char	rbuf[1025], wbuf[42];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void	notify(int from, char *s)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (FD_ISSET(fd, &wfds) && fd != from)
			send(fd, s, strlen(s), 0);
	}
}

void	add_client(int fd)
{
	max_fd = fd > max_fd ? fd : max_fd;
	ids[fd] = clients++;
	msg[fd] = NULL;
	FD_SET(fd, &fds);
	sprintf(wbuf, "server: client %d just arrived\n", ids[fd]);
	notify(fd, wbuf);
}

void	remove_client(int fd)
{
	sprintf(wbuf, "server: client %d just left\n", ids[fd]);
	notify(fd, wbuf);
	free(msg[fd]);
	msg[fd] = NULL;
	FD_CLR(fd, &fds);
	close(fd);
}

void	deliver(int fd)
{
	char	*s;

	while (extract_message(&(msg[fd]), &s))
	{
		sprintf(wbuf, "client %d: ", ids[fd]);
		notify(fd, wbuf);
		notify(fd, s);
		free(s);
		s = NULL;
	}
}

int	create_socket(void)
{
	max_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (max_fd < 0)
		fatal();
	FD_SET(max_fd, &fds);
	return (max_fd);
}

int	main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	FD_ZERO(&fds);
	int sockfd = create_socket();
	
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(av[1])); // replace 8080

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
		fatal();
	if (listen(sockfd, SOMAXCONN)) // the main uses 10, SOMAXCONN is 180 on my machine
		fatal();
	
	while (1)
	{
		rfds = wfds = fds;

		if (select(max_fd + 1, &rfds, &wfds, NULL, NULL) < 0)
			fatal();

		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (!FD_ISSET(fd, &rfds))
				continue;
			if (fd == sockfd)
			{
				socklen_t	addr_len = sizeof(servaddr);
				int	client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &addr_len);
				if (client_fd >= 0)
				{
					add_client(client_fd);
					break;
				}
			}
			else
			{
				int readed = recv(fd, rbuf, 1024, 0);
				if (readed <= 0)
				{
					remove_client(fd);
					break;
				}
				rbuf[readed] = '\0';
				msg[fd] = str_join(msg[fd], rbuf);
				deliver(fd);
			}
		}
	}
	return (0);
}





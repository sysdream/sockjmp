/*

 GNU GPL LICENSE v3


 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>


 Author: Damien "virtualabs" Cauquil <d.cauquil@sysdream.com>

*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#ifdef DEBUG
#define _d(x...) fprintf(stdout,x)
#else
#define _d(x...)
#endif

#define _e(x...) fprintf(stderr,x)

#define KEY '\x3A'

/*
 * Types
 */

typedef enum {SOCKET_UNDEF,SOCKET_IN, SOCKET_OUT} TSocketType;

typedef enum {ENC_DISABLED,ENC_ENABLED} TSocketEnc;

typedef struct tSocket {
	char hostname[256];
	unsigned int port;
	unsigned int sport;
	TSocketType type;
	TSocketEnc encryption;
	int sock;
	int sock_slave;
} TSocket;

/*
 * Helpers
 */
 
unsigned long name_resolve(char *host_name)
{
	struct in_addr addr;
	struct hostent *host_ent;
	if((addr.s_addr=inet_addr(host_name))==-1) {
    	host_ent=(struct hostent *)gethostbyname(host_name);
    	if(host_ent==NULL)
    		return -1;
    	memcpy((void *)host_ent->h_addr, (void *)&addr.s_addr, host_ent->h_length);
    }
	return (addr.s_addr);
}


void encrypt_decrypt_buf(char *buffer, int size, char key)
{
	int i;
	for (i=0;i<size;i++)
		buffer[i]^=key;
}

int highest_sock(TSocket *in, TSocket *out)
{
	int high_sock = -1;
	
	if (in->type==SOCKET_IN)
	{
		if (in->sock_slave>high_sock)
			high_sock = in->sock_slave;
		else
			if (in->sock>high_sock)
				high_sock = in->sock;
	}
	else
		if (in->sock>high_sock)
			high_sock = in->sock;
		
	if (out->type==SOCKET_IN)
	{
		if (out->sock_slave>high_sock)
			high_sock = out->sock_slave;
		else
			if (out->sock>high_sock)
				high_sock = out->sock;
	}
	else
		if (out->sock>high_sock)
			high_sock = out->sock;

	return high_sock;
}

void socket_send(TSocket *sock, char *buffer, int size)
{
	/* encrypt or decrypt buffer if required */
	if (sock->encryption==ENC_ENABLED)
		encrypt_decrypt_buf(buffer, size, KEY);

	/* send data */
	if (sock->type==SOCKET_OUT)
		write(sock->sock, buffer, size);
	else if (sock->sock_slave>0)
		write(sock->sock_slave, buffer, size);
}

TSocket *parse_arg(char *arg)
{
	char *cur = NULL, *off=NULL;
	TSocket *sock_info = NULL;	
	
	/* allocate memory */
	sock_info = (TSocket *)malloc(sizeof(TSocket));
	if (sock_info != NULL)
	{
		/* fill in the structure */
		memset(sock_info->hostname, 0, 256);
		sock_info->port = 0;
		sock_info->sport = 0;
		sock_info->type = SOCKET_UNDEF;
		sock_info->sock = -1;
		sock_info->sock_slave = -1;
		sock_info->encryption = ENC_DISABLED;
		
		/* parse argument [i|o]:IP:port */
		cur = arg;
		switch (*cur)
		{
			case 'x':
				{
					/* socket type is IN with encryption enabled */
					if (sock_info->type == SOCKET_UNDEF)
					{
						_d("Socket type is IN+ENC\n");
						sock_info->type = SOCKET_IN;
						sock_info->encryption = ENC_ENABLED;
					}
				}
			case 'z':
				{
					/* socket type is OUT with encryption enabled */
					if (sock_info->type == SOCKET_UNDEF)
					{
						_d("Socket type is OUT+ENC\n");
						sock_info->type = SOCKET_OUT;
						sock_info->encryption = ENC_ENABLED;					
					}
				}
			case 'i':
				{
					/* socket type is IN */
					if (sock_info->type == SOCKET_UNDEF)
					{
						_d("Socket type is IN\n");
						sock_info->type = SOCKET_IN;
					}
				}
			case 'o':
				{
					/* socket type is OUT */
					if (sock_info->type == SOCKET_UNDEF)
					{
						_d("Socket type is OUT\n");
						sock_info->type = SOCKET_OUT;
					}
						
					/* if next char is not ':' but a number, LPORT is specified */
					cur++;
					_d("TCHAR: %c\n",*cur);
					if (((*cur)!=':') && ((*cur)>='0') && ((*cur)<='9'))
					{
						off = cur;
						while ((*off)&&(*off!=':')&&(*off>='0')&&(*off<='9'))
							off++;
						if (*off==':')
						{
							*off='\0';
							sock_info->sport = atoi(cur);
							_d("Local port set to: %d", sock_info->sport);
							*off=':';
							cur = off;
						}
						else
						{
							_d("Oops\n");
							free(sock_info);
							return NULL;
						}
					}	
						
					/* next char must be ':' */
					if (*cur!=':')
					{
						_d("Oops 1\n");
						/* free sock_info and return NULL */
						free(sock_info);
						return NULL;
					}
					else
					{
						/* followed by an ip address or a hostname */
						off = ++cur;
						while ((*off)&&(*off!=':'))
							off++;
						if (*off==':')
						{
							/* save hostname */
							*off = '\0';
							strncpy(sock_info->hostname, cur, 256);
							_d("Hostname is: %s\n",sock_info->hostname);
							
							/* and a port (decimal) */
							cur = ++off;
							if (*cur==0)
							{
								/* free sock_info and return NULL */
								return NULL;
							}
							else
							{
								/* try to convert it to an unsigned integer */
								sock_info->port = atoi(cur);
								if ( ((sock_info->port<=0) && sock_info->type==SOCKET_OUT) || ((sock_info->port<=1024) && sock_info->type==SOCKET_IN))
								{
									/* free sock_info and return NULL */
									free(sock_info);
									return NULL;
								}
								else
								{
									_d("Port is %d\n",sock_info->port);
									return sock_info;
								}
							}
						}
						else
						{
							_d("Oops 2\n");
							/* free sock_info and return NULL */
							free(sock_info);
							return NULL;
						}
					}
				}
				break;
								
			default:
				return NULL;
		}
		
	}
	else
		return NULL;	/* cannot allocate memory */
}

/*
 * Server stuff
 */
 
int server(TSocket *sock)
{
	int x;
	int reuse_addr = 1;
	struct sockaddr_in sin;

	sock->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock->sock<0)
		return 0;
		
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family=AF_INET;
	sin.sin_port=htons(sock->port);
	sin.sin_addr.s_addr=htonl(INADDR_ANY);

	/* set socket reuse */
	setsockopt(sock->sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

  /* set non-blocking */
	x=fcntl(sock->sock,F_GETFL,0);
	fcntl(sock->sock,F_SETFL,x|O_NONBLOCK);	
	
	/* bind */
	if (bind(sock->sock, (struct sockaddr *)&sin, sizeof(sin))<0)
	{
		_e("Bind failed\n");
		return 0;
	}
	
	/* listen */	
	listen(sock->sock,5);

	return 1;
}


/*
 * Client stuff
 */
 
int client(TSocket *sock)
{
	int x;
	struct sockaddr_in sin,lin;
	int sock_opt_val=1;

	sock->sock = socket(AF_INET, SOCK_STREAM, 0);

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family=AF_INET;
	sin.sin_port=htons(sock->port);
	sin.sin_addr.s_addr=name_resolve(sock->hostname);
	
	if (sin.sin_addr.s_addr==0)
		return 0;

	/* if source port is specified, then bind the client socket to this port */
	if (sock->sport>0)
	{
		/* set a new sockaddr_in structure */
		memset(&lin, 0, sizeof(struct sockaddr_in));
		lin.sin_family=AF_INET;
		lin.sin_port=htons(sock->sport);
		lin.sin_addr.s_addr=INADDR_ANY;

		/* set socket option SO_REUSEADDR */
		if (setsockopt(sock->sock, SOL_SOCKET, SO_REUSEADDR, &sock_opt_val, sizeof sock_opt_val)<0)
		{
			_e("Cannot set REUSE_ADDR option\n");
			return 0;
		}

		/* bind socket to this port */
		_d("Binding local port to %d\n", sock->sport);
		if (bind(sock->sock, (struct sockaddr *)&lin, sizeof(struct sockaddr_in))<0)
		{
			_e("Cannot bind socket to local port %d.\n", sock->sport);
			return 0;
		}
	}

	/* connect socket to destination */
	x=connect(sock->sock, (struct sockaddr *)&sin, sizeof(sin));
	if(x<0)
		return 0;

	x=fcntl(sock->sock,F_GETFL,0);
	fcntl(sock->sock,F_SETFL,x|O_NONBLOCK);

	return 1;
}

/*
 * Bridging routines
 */

int handle_new_connection(TSocket *sock)
{
	int x;
	
	sock->sock_slave = accept(sock->sock, NULL, NULL);
	if (sock->sock_slave<0)
		return 0;
	else
	{
		x=fcntl(sock->sock_slave,F_GETFL,0);
		fcntl(sock->sock_slave,F_SETFL,x|O_NONBLOCK);
	}
	
	return 1;
}

int handle_bridging(fd_set *socks, TSocket *from, TSocket *to)
{
	char inbuff[512];
	int nbin;	

	/* something to forward from in to out ? */
	if(FD_ISSET(from->sock, socks))
	{	
		/* is this socket awaiting for connections ? */
		if ((from->type==SOCKET_IN) && (from->sock_slave<0))
		{
			FD_CLR(from->sock, socks);
			_d("New connection ...\n");
			if (!handle_new_connection(from))
				return 0;
		}
		else if (((from->type==SOCKET_IN) && (to->sock_slave>0)) || (from->type==SOCKET_OUT))
		{
			FD_CLR(from->sock, socks);
			memset(&inbuff,0,sizeof(inbuff));
			nbin = read(from->sock, inbuff, sizeof(inbuff)-1);
			if (nbin <= 0)
			{
				_e("[!] Remote end closes connection.\n");
				close(from->sock);
				return 0;
			}
			else
			{

				/* decrypt if necessary */
				if (from->encryption==ENC_ENABLED)
					encrypt_decrypt_buf(inbuff, nbin, KEY);

				/* forward to the other socket */
				socket_send(to, inbuff, nbin);
			}
		}
	}
		
	if ((from->type==SOCKET_IN) && FD_ISSET(from->sock_slave, socks))
	{
		if ((to->type==SOCKET_OUT) || ((to->type==SOCKET_IN) && to->sock_slave>0))
		{
			FD_CLR(from->sock_slave, socks);	

			memset(&inbuff,0,sizeof(inbuff));
			nbin = read(from->sock_slave, inbuff, sizeof(inbuff)-1);
			if (nbin <= 0)
			{
				_e("[!] Remote end closes connection.\n");
				close(from->sock_slave);
				return 0;
			}
			else
			{

				/* decrypt if necessary */
				if (from->encryption==ENC_ENABLED)
					encrypt_decrypt_buf(inbuff, nbin, KEY);				
				
				/* send to the other socket */
				socket_send(to, inbuff, nbin);
			}			
		}
	}
	
	return 1;
}



/*
 * Main
 */

int main (int argc, char **argv)
{
	fd_set read_flags;
	struct timeval waitd;
	TSocket *master_in,*master_out;
	int err,nfd=0;          

	/* check args */
	if (argc != 3)
	{
		_e("Usage: %s [i|o|x](LPORT):IP:PORT [i|o|x](LPORT):IP:PORT\n", argv[0]);
		return -1;
	}
	
	/* parse args */
	master_in = parse_arg(argv[1]);
	if (!master_in)
	{
		_e("Syntax error in arg #1.\n");
		return -1;
	}
	else
	{
		/* create master in socket */
		if (master_in->type==SOCKET_OUT)
		{
			if(!client(master_in))
			{
				_e("Unable to connect to %s:%d\n",master_in->hostname,master_in->port);
				return -1;
			}
		}
		else
		{
			if(!server(master_in))
			{
				_e("Unable to listen from %s:%d\n",master_in->hostname,master_in->port);
				return -1;
			}
		}
	}
	
	master_out = parse_arg(argv[2]);
	if (!master_out)
	{
		_d("Syntax error in arg #2.\n");
		return -1;
	}	
	else
	{
		/* create master in socket */
		if (master_out->type==SOCKET_OUT)
		{
			if(!client(master_out))
			{
				_e("Unable to connect to %s:%d\n",master_out->hostname,master_out->port);
				return -1;
			}
		}
		else
		{
			if(!server(master_out))
			{
				_e("Unable to listen from %s:%d\n",master_out->hostname,master_out->port);
				return -1;
			}
		}

	}
		
	/* select the socket */
	while(1) {
		
		waitd.tv_sec = 3;
		waitd.tv_usec = 0;
		nfd = 0;
		FD_ZERO(&read_flags);

		/* handles master in */
		if (master_in->type==SOCKET_IN)
		{
			if (master_in->sock_slave>0)
				FD_SET(master_in->sock_slave, &read_flags);
			else
				FD_SET(master_in->sock, &read_flags);
		}
		else
			FD_SET(master_in->sock, &read_flags);
		
		/* handles master out */		
		if (master_out->type==SOCKET_IN)
		{
			if (master_out->sock_slave>0)
				FD_SET(master_out->sock_slave, &read_flags);
			else
				FD_SET(master_out->sock, &read_flags);
		}
		else
			FD_SET(master_out->sock, &read_flags);
		
		/* select sockets */
		err=select(highest_sock(master_in, master_out)+1, &read_flags,NULL,(fd_set*)0,&waitd);
		if(err <= 0)
			continue;
		else
		{
			if(!handle_bridging(&read_flags, master_in, master_out))
				break;
			if(!handle_bridging(&read_flags, master_out, master_in))
				break;
		}
	}

	return 0;
}
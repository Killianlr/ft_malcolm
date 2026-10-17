/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_itoa.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kle-rest <kle-rest@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/11/11 15:05:48 by kle-rest          #+#    #+#             */
/*   Updated: 2022/11/21 14:23:17 by kle-rest         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static char	*ft_reverse(char *nb, int c)
{
	char	temp;
	int		i;

	i = 0;
	while (i < c / 2)
	{
		temp = nb[i];
		nb[i] = nb[c - i - 1];
		nb[c - i - 1] = temp;
		i++;
	}
	return (nb);
}

static int	ft_count(int n)
{
	int	i;

	i = 1;
	while (n >= 10)
	{
		i++;
		n = n / 10;
	}
	return (i);
}

static char	*ft_convertnb(char *nb, int n, int c, int s)
{
	int	i;

	i = 0;
	nb[i] = n % 10 + 48;
	i++;
	while (n >= 10)
	{
		n = n / 10;
		nb[i] = n % 10 + 48;
		i++;
	}
	if (s == 1)
	{
		nb[i] = '-';
		i++;
		c++;
	}
	nb[i] = 0;
	ft_reverse(nb, c);
	return (nb);
}

char	*ft_itoa(int n)
{
	int		s;
	int		c;
	char	*nb;

	if (n == -2147483648)
		return (ft_strdup("-2147483648"));
	if (n < 0)
	{
		s = 1;
		n *= -1;
	}
	else
		s = 0;
	c = ft_count(n);
	nb = malloc(sizeof(char) * (c + s + 1));
	if (!nb)
		return (NULL);
	ft_convertnb(nb, n, c, s);
	return (nb);
}

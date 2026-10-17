/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_split.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kle-rest <kle-rest@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/11/11 00:13:40 by kle-rest          #+#    #+#             */
/*   Updated: 2022/11/22 14:45:28 by kle-rest         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static int	ft_countword(const char *s, char c)
{
	int	count;
	int	i;
	int	word;

	count = 0;
	i = 0;
	word = 0;
	while (s[i])
	{
		if (s[i] == c)
			word = 0;
		if (word == 0 && s[i] != c)
		{
			count++;
			word = 1;
		}
		i++;
	}
	return (count);
}

static int	ft_sizeword(const char *s, char c, int i)
{
	int	size;

	size = 0;
	while (s[i] && s[i] != c)
	{
		size++;
		i++;
	}
	return (size);
}

static char	*ft_stricpy(char *dest, const char *src, char c, int i)
{
	int	j;

	j = 0;
	while (src[i] && src[i] != c)
	{
		dest[j] = src[i];
		i++;
		j++;
	}
	dest[j] = '\0';
	return (dest);
}

static char	**ft_fill_tab(char **tab, const char *s, char c)
{
	int	r;
	int	i;
	int	w_len;

	r = 0;
	i = 0;
	while (r < ft_countword(s, c))
	{
		while (s[i] && s[i] == c)
			i++;
		w_len = ft_sizeword(s, c, i);
		tab[r] = malloc(sizeof(char) * (w_len + 1));
		if (!tab[r])
			return (NULL);
		tab[r] = ft_stricpy(tab[r], s, c, i);
		i += w_len;
		r++;
	}
	tab[r] = NULL;
	return (tab);
}

char	**ft_split(char const *s, char c)
{
	char	**tab;

	tab = malloc(sizeof(char *) * (ft_countword(s, c) + 1));
	if (!tab)
		return (NULL);
	tab = ft_fill_tab(tab, s, c);
	return (tab);
}

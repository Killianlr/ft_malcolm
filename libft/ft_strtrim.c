/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strtrim.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kle-rest <kle-rest@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/11/10 17:59:08 by kle-rest          #+#    #+#             */
/*   Updated: 2022/11/18 14:07:52 by kle-rest         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static int	ft_checksetav(char const *s1, char const *set)
{
	int	i;
	int	j;
	int	e;

	j = 0;
	i = 0;
	e = 1;
	while (s1[i])
	{
		e = 0;
		while (set[j])
		{
			if (set[j] == s1[i])
				e = 1;
			j++;
		}
		i++;
		j = 0;
		if (e == 0)
			return (i - 1);
	}
	return (i - 1);
}

static int	ft_checksetar(char const *s1, char const *set, int k)
{
	int	i;
	int	j;
	int	e;

	j = 0;
	i = (ft_strlen(s1) - 1);
	e = 1;
	while (i > 0 && i >= k)
	{
		e = 0;
		while (set[j])
		{
			if (set[j] == s1[i])
				e = 1;
			j++;
		}
		if (e == 0)
			return (i + 1);
		else
		{	
			i--;
			j = 0;
		}
	}
	return (i + 1);
}

char	*ft_strtrim(char const *s1, char const *set)
{
	char	*str;
	int		i;
	int		j;
	int		len;
	int		u;

	if (!ft_strncmp(s1, "", 1))
		return (ft_strdup(""));
	j = 0;
	i = ft_checksetav(s1, set);
	u = ft_checksetar(s1, set, i);
	len = u - i;
	str = malloc(sizeof(char) * (len + 1));
	if (!str)
		return (0);
	while (i < u)
	{
		str[j] = s1[i];
		i++;
		j++;
	}
	str[j] = 0;
	return (str);
}

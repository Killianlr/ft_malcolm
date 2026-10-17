/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strrchr.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kle-rest <kle-rest@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/11/09 13:50:34 by kle-rest          #+#    #+#             */
/*   Updated: 2022/11/16 12:50:10 by kle-rest         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_strrchr(const char *s, int c)
{
	int		i;
	char	*sc;

	i = 0;
	if (!s)
		return (0);
	sc = (char *)s;
	if (c > 127)
		return ((char *)s);
	while (sc[i])
		i++;
	while (i > 0)
	{
		if (sc[i] == c)
			return (&sc[i]);
		i--;
	}
	if (sc[i] == c)
		return (&sc[i]);
	return (0);
}

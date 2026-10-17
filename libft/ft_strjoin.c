/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strjoin.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kle-rest <kle-rest@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/11/10 17:39:57 by kle-rest          #+#    #+#             */
/*   Updated: 2022/11/18 14:06:57 by kle-rest         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_strjoin(char const *s1, char const *s2)
{
	size_t	i;
	size_t	j;
	char	*s12;
	size_t	len;

	i = 0;
	j = 0;
	if (!s1 || !s2)
		return (NULL);
	len = ft_strlen(s1) + ft_strlen(s2);
	s12 = malloc(sizeof(char) * (len + 1));
	if (!s12)
		return (0);
	while (i < len)
	{
		if (i < ft_strlen(s1))
			s12[i] = s1[i];
		else
			s12[i] = s2[j++];
		i++;
	}
	s12[i] = 0;
	return (s12);
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_substr.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kle-rest <kle-rest@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/11/10 17:20:08 by kle-rest          #+#    #+#             */
/*   Updated: 2022/11/21 14:24:05 by kle-rest         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_substr(char const *s, unsigned int start, size_t len)
{
	size_t	i;
	size_t	j;
	char	*sm;

	i = 0;
	j = (size_t)start;
	if (!s)
		return (0);
	if (start >= ft_strlen(s))
		return (ft_strdup(""));
	if (ft_strlen(s) - start < len)
		len = ft_strlen(s) - start;
	sm = malloc(sizeof(char) * (len + 1));
	if (!sm)
		return (0);
	while (len > 0)
	{
		sm[i] = s[j];
		i++;
		j++;
		len--;
	}
	sm[i] = '\0';
	return (sm);
}

﻿return {
	ret = "string",
	retDesc = <<<END'
Соединенные значения элементов массива.
END,
	desc = <<<END"
Объединяет значения всех элементов массива, вставляя между ними заданную строку-разделитель.
END,
	params = {
		separator = {
			type = "string",
			def = '""',
			desc = "Разделитель",
		},
	},
}
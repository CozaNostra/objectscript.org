﻿return {
	// name = 'functionOf',
	desc = "Функция проверки совместимости значения на функцию.",
	ret = 'function',
	retDesc = <<<END"
<p>Возвращает результат проверки параметра на функцию.
<ul>
<li><p>Если тип параметра является функция, то его значение будет возвращено.
<li><p>Для др. типов возвращается <code>null</code>.
</ul>
END,			
	params = {
		value = {
			type = 'mixed',
			desc = 'Тестируемое значение',
		},
	},			
}
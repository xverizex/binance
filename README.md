# binance info

Программа бот. Отслеживание курса на криптобирже. в планах добавить другие валюты. пока есть только биткоин. если кто хочет помочь, то подскажите стратегии поиска точки входа. и интересуют другие стратегии, которые можно применить в алгоритмах для бота.

Функционал.
* процент повышения - можно указать процент повышения. если цена повыситься на столько-то процентов, то бот оповестит пользователя.
* нижний порог - здесь надо указать от какой цены отслеживать процент. 
* точка входа - я реализовал не правильный алгоритм. как работает этот алгоритм. указываешь процент 10 например. если цена биткоина становиться меньше на 10 процентов, то запускается слежение. если цена опять поднимется до нижнего порога, то будет оповещение.

если `configure` ругается, что нет нужных библиотек, то установите их. так же если версии библиотек отличаются, то внесите правки в файл `configure`. надо ещё сделать такой `configure`, чтобы он определял версии библиотек самостоятельно.

Установка.
```
./configure
make
sudo make install
```

![](https://i.imgur.com/L3NdPjO.png)

есть видео https://www.youtube.com/watch?v=RzDk3O8wL3U&feature=youtu.be

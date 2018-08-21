# bmp to jpeg converter
This is yet another image format converter from bmp to jpeg,
but easily translatable to hardware description language.

## How to compile
Require: recent g++
```
make
```

## How to use
```
./a.out <bmp image file>
```
Converted jpeg will be placed on /tmp/out.jpg .

## License
Copyright (c) 2018 ikanoano  
Released under the [MIT](https://opensource.org/licenses/mit-license.php) license

## References
[Recommendation T.81](https://www.w3.org/Graphics/JPEG/itu-t81.pdf)  
[Tech と Culture - ハフマン符号のアルゴリズム, etc.](http://kurinkurin12.hatenablog.com/entry/20100110/1263131721)  
[しらぎくさいと実験室。 - JPEG画像形式の概要(圧縮アルゴリズム)。](https://www.marguerite.jp/Nihongo/Labo/Image/JPEG.html)  
[Fussy's Homepage - 圧縮アルゴリズム (7) JPEG法 (2)](http://fussy.web.fc2.com/algo/compress7_jpeg2.htm)


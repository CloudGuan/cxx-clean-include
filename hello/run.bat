@echo off
:: ʹ��˵����
:: 
:: 1.  �������Ŀ��visual studio��Ŀ(2005�����ϰ汾�������ļ���vcproj��vcxproj��׺������ʹ���������
::         ..\cxxclean.exe -clean ���vs��Ŀ
::
::     ���磺..\cxxclean.exe -clean ./hello.vcxproj
::         
::
:: 2.  �������Ŀ����vs���̣�������һ���ļ��е��£���ʹ���������
::         ..\cxxclean.exe -clean �����Ŀ�����ļ���
::
::     ���磺..\cxxclean.exe -clean ./
::
::
:: 3.  ����鿴html��־���������к�ɫ���������ʾ�Ҳ���ͷ�ļ�·���������Ҳ���ĳ����ʱ����ʹ�����·�ʽ��ע�����--�ţ���
::         ..\cxxclean -clean �ļ���·�� -- -I"ͷ�ļ�����·��" -D ��ҪԤ����ĺ� -include ��Ҫǿ�ư������ļ�
::         �����У�-I��-D��-include����ʹ�ö�Σ�
::
::     ���磺cxxclean -clean d:/a/b/hello/ -- -I"../../" -I"../" -I"./" -I"../include" -D DEBUG -D WIN32 -include platform.h > cxxclean_hello.html

..\cxxclean.exe -clean ./hello.vcxproj
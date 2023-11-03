Sub abc(x)
  If x > 0 Then
    abc = abc(x-1) + x
  Else
    abc = 0
  EndIf
EndSub

Print abc(6)

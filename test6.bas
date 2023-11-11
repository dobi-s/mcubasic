Sub Test(a())
  a(1) = 321
  Test2 a
End Sub

Sub Test2(b())
  Print b(1)
  b(2) = 456
End Sub

Dim x(3)
x(0) = 123

Test x
Print x(1)

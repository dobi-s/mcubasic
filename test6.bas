Sub Test()
  Dim a
  Dim b(3)
  Dim c

  a = 9
  b(0) = 12
  b(2) = 13
  c = 999

  Test2
  Print b(2)
End Sub

Sub Test2()
  Dim a
  Dim b(3)
  Dim c

  a = 9
  b(0) = 12
  b(2) = 15
  c = 999

  Print b(2)
End Sub

Test

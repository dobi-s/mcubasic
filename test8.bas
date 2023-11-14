Dim a
Dim b

Sub Test(x)
  Return ReadData(x)
End Sub

Sub main()
  Dim data(8)
  Dim id

  id = Test(data)
  If id >= 0 Then
    Print "Data = ";
    For i = 0 To 7
      Print data(i); " ";
    Next
    Print ""
  Else
    Print "Id = "; id
  End IF
End Sub

main

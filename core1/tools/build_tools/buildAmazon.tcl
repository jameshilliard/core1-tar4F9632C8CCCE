/usr/bin/wish

#frame .f1
#frame .f2
#pack .f1 .f2

button .b1 -text "START" -command "destroy ."
button .b2 -text "QUIT" -command "destroy ."
grid .b1 -row 0 -column 2
grid .b2 -row 1 -column 2

label .l -text "Options:"
entry .e -width 20 -relief sunken -bd 2 -textvariable name
focus .e
button .clear1 -text Clear -command {set name ""}
grid .l -row 0 -column 3
grid .e -row 0 -column 4
grid .clear1 -row 1 -column 4
scrollbar .scroll -command ".t yview"
text .t -yscrollcommand ".scroll set" -setgrid true -width 40 -height 10 -wrap word
grid .scroll -row 0 -column 0 -rowspan 10
grid .t -row 0 -column 1 -columnspan 10


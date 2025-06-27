let print_usage () =
  print_endline "exp.exe <FILE>";;

let main () =
  let arg_count = Array.length Sys.argv in
  if arg_count != 2 then
    begin
      print_usage ();
      exit 1;
    end;
  let requested_file = Array.get Sys.argv 1 in
  if not (Sys.file_exists (requested_file) && Sys.is_regular_file (requested_file)) then
    begin
      print_endline "Requested file does not exist";
      exit 1;
    end;
  let file_input = open_in requested_file in
  try
    let line = input_line file_input in
    print_endline line;
    flush stdout;
    close_in file_input;
  with _ ->
    close_in_noerr file_input;
  exit 0;;

main ();;

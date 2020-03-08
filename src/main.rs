use std::{ io, process };
use std::io::Write;

fn main() {
    loop {
        let mut query = String::new();

        print!("rusqlite> ");
        // printはデフォルトでラインバッファに溜まるので明示的にflushする
        io::stdout().flush().unwrap();

        io::stdin().read_line(&mut query).expect("failed to read line");
        if query.trim_end() == ".exit" {
            println!("bye✋");
            process::exit(0);
        } else {
            println!("Unrecognized command: {}", query);
        }
    }
}

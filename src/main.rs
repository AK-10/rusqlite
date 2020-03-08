use std::{ io, process };
use std::io::Write;

// use crate::meta_command::{ MetaCommand, MetaCommandResult };
// use crate::statement;

#[derive(Debug)]
enum MetaCommandResult {
    Success,
    UnrecognizedCommand,
}

#[derive(Debug)]
struct MetaCommand {}

impl MetaCommand {
        pub fn process(command: String) -> MetaCommandResult {
        if command == ".exit" {
            // .exit
            MetaCommandResult::Success
        } else {
            MetaCommandResult::UnrecognizedCommand
        }
    }
}

#[derive(Debug)]
enum StatementType {
    Insert,
    Select,
    Error,
}

struct Statement {
    statement_type: StatementType,
}

impl Statement {
    pub fn new(token: String) -> Statement {
        // String -> &strへ変換
        match &*token {
            "insert" => Statement {
                statement_type: StatementType::Insert,
            },
            "select" => Statement {
                statement_type: StatementType::Select,
            },
            _ => Statement {
                statement_type: StatementType::Error,
            }
        }
    }
}

fn main() {
    loop {
        let mut input_str = String::new();

        print!("rusqlite> ");
        // printはデフォルトでラインバッファに溜まるので明示的にflushする
        io::stdout().flush().unwrap();

        io::stdin().read_line(&mut input_str).expect("failed to read line");
        let command_str = input_str.trim_end();

        let command = &command_str;
        // '.exit'のようなsqlでないコマンドをメタコマンドとする.
        if let Some('.') = command.chars().nth(0) {
            match MetaCommand::process(command.to_string()) {
                MetaCommandResult::Success => {
                    println!("bye✋");
                    process::exit(0);
                },
                _ => {
                    println!("unrecognized command {}", command);
                }
            };
            continue;
        }

        // 最初のトークンで
        let first_token = command.split_whitespace().nth(0).unwrap_or("").to_string();
        let statement = Statement::new(first_token);
        match statement.statement_type {
            StatementType::Insert => {
                println!("This is where we would do an insert.");
            },
            StatementType::Select => {
                println!("This is where we would do an select.");
            },
            _ => {
                continue;
            }
        }

        // execute_statement(statement);
        println!("executed.");
    }
}

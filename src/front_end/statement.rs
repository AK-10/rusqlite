mod statement {
    #[derive(Debug)]
    pub enum StatementType {
        Insert,
        Select,
        Error,
    }

    pub struct Statement {
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
}

use pqc_kyber::{KYBER_90S, KYBER_K};
use std::fs::File;
use std::io::{prelude::*, BufReader};
use std::path::PathBuf;

// Known Answer Tests
#[derive(Debug)]
pub struct Kat {
    // Deterministic RNG buffers
    pub keygen_buffer1: String,
    pub keygen_buffer2: String,
    pub encap_buffer: String,
    // Keys, Ciphertext and Shared Secret
    pub pk: String,
    pub sk: String,
    pub ct: String,
    pub ss: String,
}

// Converts string octuples from tvec files into Kat structs
impl From<&[String]> for Kat {
    fn from(kat: &[String]) -> Self {
        // Extract values from key:value lines
        let values: Vec<String> = kat
            .iter()
            .map(|katline| {
                let val: Vec<&str> = katline.split(": ").collect();
                if val.len() > 1 {
                    val[1].into()
                } else {
                    val[0].into()
                }
            })
            .collect();
        // Build KAT from values
        Kat {
            keygen_buffer1: values[0].clone(),
            keygen_buffer2: values[1].clone(),
            pk: values[2].clone(),
            sk: values[3].clone(),
            encap_buffer: values[4].clone(),
            ct: values[5].clone(),
            ss: values[6].clone(),
        }
    }
}

// Get KAT filename based on security level and if 90s mode
fn get_filename() -> String {
    let mut filename = match KYBER_K {
        2 => "tvecs512".to_string(),
        3 => "tvecs768".to_string(),
        4 => "tvecs1024".to_string(),
        _ => panic!("No security level set"),
    };
    if KYBER_90S {
        filename.push_str("-90s");
    }
    println!("Using KAT file: {}", &filename);
    filename
}

// Base dir
fn get_test_dir() -> PathBuf {
    let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    path.extend(&["tests"]);
    path
}

// KATs path
fn get_kat_filepath() -> PathBuf {
    let mut path = get_test_dir();
    path.extend(&["KATs"]);
    path.extend(&[get_filename()]);
    path
}

fn load_file(filepath: PathBuf) -> File {
    File::open(filepath).expect("Error loading KAT file")
}

fn parse_kats() -> Vec<String> {
    let file = load_file(get_kat_filepath());
    let buf = BufReader::new(file);
    buf.lines().map(|l| l.expect("Unable to parse line")).collect()
}

// Packs chunks of lines into Kat structs
pub fn build_kats() -> Vec<Kat> {
    let lines = parse_kats();
    let kats = lines.chunks_exact(8);
    // Map String slices into Vec<KAT>
    kats.map(|c| c.into()).collect::<Vec<Kat>>()
}

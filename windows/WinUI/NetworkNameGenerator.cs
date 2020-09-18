using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WinUI
{
    internal class NetworkNameGenerator
    {
        public static string GenerateName()
        {
            Random r = new Random(DateTime.Now.Millisecond);
            int firstIndex = r.Next(0, FIRST.Length);
            int secondIndex = r.Next(0, SECOND.Length);
            return FIRST[firstIndex] + "_" + SECOND[secondIndex];
        }

        private static string[] FIRST =
        {
              "admiring",
              "adoring",
              "agitated",
              "amazing",
              "angry",
              "awesome",
              "berserk",
              "big",
              "clever",
              "compassionate",
              "cranky",
              "crazy",
              "desperate",
              "determined",
              "distracted",
              "dreamy",
              "ecstatic",
              "elated",
              "elegant",
              "fervent",
              "focused",
              "furious",
              "gigantic",
              "gloomy",
              "goofy",
              "grave",
              "happy",
              "high",
              "hopeful",
              "hungry",
              "insane",
              "jolly",
              "jovial",
              "lonely",
              "loving",
              "modest",
              "nostalgic",
              "pedantic",
              "pensive",
              "prickly",
              "reverent",
              "romantic",
              "sad",
              "serene",
              "sharp",
              "silly",
              "sleepy",
              "stoic",
              "stupefied",
              "suspicious",
              "tender",
              "thirsty",
              "tiny",
              "trusting"
        };

        private static string[] SECOND =
        {
            // constructed telephone-like devices in 1854
            "meucci",

            // prototype make-or-break telephones in 1860
            "reis",

            // Alexander Graham Bell
            "bell",

            // designed telephone using water microphone in 1876
            "gray",

            // Tivadar Puskás invented the telephone switchboard exchange in 1876.
            "puskas",

            // Thomas Edison, invented the carbon microphone which produced a strong telephone signal.
            "edison",

            // 1950s, Paul Baran developed the concept Distributed Adaptive Message Block Switching
            "baran",

            // Donald Davies coined the phrase 'packet switching network'
            "davies",

            // Robert Licklider helped get ARPANET funded
            "licklider",

            // Robert Taylor, ARPANET pioneer
            "taylor",

            // Lawrence Roberts, ARPANET
            "roberts",

            // Vint Cerf, TCP
            "cerf",

            // Bob Kahn, TCP
            "kahn",

            // David P Reed, UDP
            "reed",

            // Community Memory was created by Efrem Lipkin, Mark Szpakowski, and Lee Felsenstein, acting as The Community Memory Project within the Resource One computer center at Project One in San Francisco.
            "lipkin",
            "szpakowski",
            "felsenstein",

            // The first public dial-up BBS was developed by Ward Christensen and Randy Suess.
            "christensen",
            "suess",

            // Joybubbles (May 25, 1949 – August 8, 2007), born Josef Carl Engressia, Jr. in Richmond, Virginia, USA, was an early phone phreak.
            "engressia",
            "joybubbles",

            // John Thomas Draper (born 1943), also known as Captain Crunch, Crunch or Crunchman (after Cap'n Crunch breakfast cereal mascot), is an American computer programmer and former phone phreak
            "draper",

            // Dennis C. Hayes, founder of Hayes Microcomputer Products
            // "The Modem of Dennis Hayes and Dale Heatherington."
            "hayes",
            "heatherington",

            // "Ethernet was developed at Xerox PARC between 1973 and 1974.[7][8] It was inspired by ALOHAnet, which Robert Metcalfe had studied as part of his PhD dissertation."
            "metcalfe",

            // William Bradford Shockley Jr. (February 13, 1910 – August 12, 1989) was an American physicist and inventor. Shockley was the manager of a research group that included John Bardeen and Walter Brattain. The three scientists invented the point contact transistor in 1947
            "shockley",
            "bardeen",
            "brattain",

            // "Randall Erck invented the modern modem as we know it today. There were devices similar to modems used by the military, but they were designed more for the purpose of sending encripted nuclear launch codes to various bases around the world."
            "erck",

            // Leonard Kleinrock, packet switching network pioneer
            "kleinrock",

            // Tim Berners-Lee, WWW
            "berners_lee",

            // Steve Wozniak, early phone phreak
            "wozniak",

            // James Fields Smathers of Kansas City invented what is considered the first practical power-operated typewriter in 1914.
            "smathers",

            // The teleprinter evolved through a series of inventions by a number of engineers, including Royal Earl House, David Edward Hughes, Emile Baudot, Donald Murray, Charles L. Krum, Edward Kleinschmidt and Frederick G. Creed.
            "house",
            "hughes",
            "baudot",
            "murray",
            "krum",
            "kleinschmidt",
            "creed",

            // Ron Rosenbaum, author of "Secrets of the Little Blue Box" which mainstreamed phone phreaking
            "rosenbaum",

            // Bram Cohen. Bram Cohen (born October 12, 1975) is an American computer programmer, best known as the author of the peer-to-peer (P2P) BitTorrent protocol,
            "cohen",

            // Jarkko Oikarinen (born 16 August 1967, in Kuusamo, Finland) is the inventor of the first Internet chat network, called Internet Relay Chat (IRC), where he is known as WiZ.
            "oikarinen",

            // "What you probably didn't know is that the author of Trumpet Winsock — Peter Tattam from Tasmania, Australia — didn't see much money for his efforts."
            "tattam",

            // Satoshi Nakamoto
            "nakamoto",

            // Philo Farnsworth, inventor of the first practical TV tube
            "farnsworth",

            // Scottish inventor John Logie Baird employed the Nipkow disk in his prototype video systems. On 25 March 1925, Baird gave the first public demonstration of televised silhouette images in motion, at Selfridge's Department Store in London.
            "baird",

            // Beginning in 1836, the American artist Samuel F. B. Morse, the American physicist Joseph Henry, and Alfred Vail developed an electrical telegraph system.
            "morse",
            "henry",
            "vail"
        };
    }
}

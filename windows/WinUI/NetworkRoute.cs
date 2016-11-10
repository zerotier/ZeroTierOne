using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    [Serializable]
    public class NetworkRoute : ISerializable
    {
        protected NetworkRoute(SerializationInfo info, StreamingContext ctx)
        {
            Target = info.GetString("Target");
            Via = info.GetString("Via");
            Flags = info.GetInt32("Flags");
            Metric = info.GetInt32("Metric");
        }

        public virtual void GetObjectData(SerializationInfo info, StreamingContext ctx)
        {
            info.AddValue("Target", Target);
            info.AddValue("Via", Via);
            info.AddValue("Flags", Flags);
            info.AddValue("Metric", Metric);
        }

        [JsonProperty("target")]
        public string Target { get; set; }

        [JsonProperty("via")]
        public string Via { get; set; }

        [JsonProperty("flags")]
        public int Flags { get; set; }

        [JsonProperty("metric")]
        public int Metric { get; set; }
    }
}

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
            Target = info.GetString("target");
            Via = info.GetString("via");
            Flags = info.GetInt32("flags");
            Metric = info.GetInt32("metric");
        }

        public virtual void GetObjectData(SerializationInfo info, StreamingContext ctx)
        {
            info.AddValue("target", Target);
            info.AddValue("via", Via);
            info.AddValue("flags", Flags);
            info.AddValue("metric", Metric);
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

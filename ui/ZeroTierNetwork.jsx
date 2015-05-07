var ZeroTierNetwork = React.createClass({
	leaveNetwork: function(event) {
		event.preventDefault();
	},

	render: function() {
		return (
			<div className="zeroTierNetwork">
				<div className="networkInfo"><span className="zeroTierAddress">{this.props.nwid}</span>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<b>[</b>&nbsp;<span>{this.props.name}</span>&nbsp;<b>]</b></div>
				<div className="networkProps">
					<div className="row">
						<div className="name">Status</div>
						<div className="value">{this.props['status']}</div>
					</div>
					<div className="row">
						<div className="name">Type</div>
						<div className="value">{this.props['type']}</div>
					</div>
					<div className="row">
						<div className="name">MAC</div>
						<div className="value zeroTierAddress">{this.props['mac']}</div>
					</div>
					<div className="row">
						<div className="name">MTU</div>
						<div className="value">{this.props['mtu']}</div>
					</div>
					<div className="row">
						<div className="name">Broadcast</div>
						<div className="value">{(this.props['broadcastEnabled']) ? 'ENABLED' : 'DISABLED'}</div>
					</div>
					<div className="row">
						<div className="name">Bridging</div>
						<div className="value">{(this.props['bridge']) ? 'ACTIVE' : 'DISABLED'}</div>
					</div>
					<div className="row">
						<div className="name">Device</div>
						<div className="value">{(this.props['portDeviceName']) ? this.props['portDeviceName'] : '(none)'}</div>
					</div>
					<div className="row">
						<div className="name">Managed&nbsp;IPs</div>
						<div className="value ipList">
							{
								this.props['assignedAddresses'].map(function(ipAssignment) {
									return (
										<div className="ipAddress">{ipAssignment}</div>
									);
								})
							}
						</div>
					</div>
				</div>
				<button className="leaveNetworkButton" onClick={this.leaveNetwork}>Leave&nbsp;Network</button>
			</div>
		);
	}
});

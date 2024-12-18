import groovy.json.JsonOutput
import java.time.Instant

def generateUUIDv7() {
    def now = Instant.now().toEpochMilli()
    def timeHex = String.format("%012x", now)
    def randomBits = UUID.randomUUID().toString().replace("-", "").substring(12)
    return "${timeHex.substring(0, 8)}-${timeHex.substring(8)}-7${randomBits.substring(0, 3)}-" +
       "${randomBits.substring(3, 7)}-${randomBits.substring(7)}"

}

def actions = [
    ["name": "set-group", "version": "v5.0.0", "args": ["validYaml", "invalidYaml"]],
    ["name": "set-group", "version": "v5.0.0", "args": []],
    ["name": "set-group", "version": "v5.0.0", "args": [""]],
    ["name": "set-group", "version": "v5.0.0", "args": ["validYaml", 8]],
    ["name": "set-group", "version": "v5.0.0", "args": ["validYaml"]],
    ["name": "update-group", "version": "v5.0.0", "args": ["noNeedArgs"]],
    ["name": "update-group", "version": "v5.0.0"],
    ["name": "update-group", "version": "v5.0.0", "args": ""]
]

def numCommands = new Random().nextInt(3)

def commands = []
if (numCommands > 0) {
    for (int i = 0; i < numCommands; i++) {
        def action = actions[new Random().nextInt(actions.size())]
        def command = [
            "document_id": generateUUIDv7(),
            "action": action,
            "target": ["type": "agent", "id": "agentID"],
            "status": "sent"
        ]
        commands << command
    }
}

if (commands.isEmpty()) {
    respond {
        withStatusCode(408)
    }
} else {
    def jsonResponse = JsonOutput.toJson(["commands": commands])
    respond {
        withStatusCode(200)
        withContent(jsonResponse)
    }
}
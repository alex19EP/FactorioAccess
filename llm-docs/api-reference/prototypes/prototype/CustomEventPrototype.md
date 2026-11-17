# CustomEventPrototype

Defines a custom event with an unique name. During runtime, mods can use the event name to [raise](runtime:LuaBootstrap::raise_event) and [subscribe to](runtime:LuaBootstrap::on_event) the custom event.

Custom events share the same namespace as custom inputs and built-in events for subscribing to and raising them.

**Parent:** [Prototype](Prototype.md)
**Type name:** `custom-event`

## Examples

```
{
  type = "custom-event",
  name = "potato-custom-event"
}
```


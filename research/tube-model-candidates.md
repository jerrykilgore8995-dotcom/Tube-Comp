# 6BC8 model uncertainty and candidate calibration

This project intentionally carries more than one 6BC8 nonlinear model:

- `V6BC8`: a **rough original engineering hypothesis** used in the first exploratory netlist. It is not calibrated.
- `V6BC8_ALT`: a five-parameter Koren-form 6BC8 model using numeric parameters published from Wayne Clay's 2014 Sylvania plate-curve digitization. Linked source: https://www.diyaudio.com/community/threads/vacuum-tube-spice-models.243950/page-115. The reported mean fit error is 0.36045 mA to that digitized curve set, **not to an Abbey Road unit**.

The alternative Koren function is an independent SPICE encoding of the mathematical relation and source-credited numerical parameters. Neither model is 'correct' until tested against 6BC8 manufacturer plate curves and in-circuit operating points. The original 436B source scan needs to be traced before selecting a model.

Calibration plan: sweep grid bias -0.5 to -10V, plate 5-275V, compare both Iplate grids, transconductance gm versus grid and plate voltages, and all valid curves from Sylvania sheet. Reject impossible plate dissipation or cutoff curves. Repeat under anode load / cathode resistor and real gain reduction. Track error as percent current plus absolute mA error. Never fit solely 1k THD.

The 6CG7 and 6AL5 currently remain preliminary candidate models. Output/input transformers are intentionally linear ideal proxies until physically documented and characterized.

class ParticleSystem {
	class Particle {
		float org [];
		float colour [];
		long die;
		Particle next;
		
		Particle (){
			org = new float[3];
			colour = new float[3];
			org[0] = 0;
			org[1] = 0;
			org[2] = 0;
			colour[0] = 0;
			colour[1] = 0;
			colour[2] = 0;
			next = null;
		}
		
		Particle (float org[], float colour[], float die){
			org = new float[3];
			colour = new float[3];
			System.arraycopy(org,0,this.org,0,3);
			System.arraycopy(colour,0,this.colour,0,3);
			this.die = (long)(die * 1000);
			next = null;
		}
	}
	
	Particle start;
	Particle end;
	
	ParticleSystem(){
		start = null;
		end = null;
	}
	
	void AddParticleColor(float org[], float time, float colour []){
		if (end==null){
			end = new Particle(org, colour, time);
			start = end;
		} else {
			Particle temp = new Particle(org, colour, time);
			end.next = temp;
			end = temp;
		}
	}
	
	void UpdateParticles(){
		long time = System.currentTimeMillis();

		if (start != null){
			for (Particle temp = start; temp.next != null; temp=temp.next){
				if (time < temp.next.die){
					//remove temp.next from loop
					temp.next = temp.next.next;
					if (temp.next == null){
						end = temp;
					}
				}
			}
			
			//special case
			if (time < start.die){
				start = start.next;
				if (start==null)	//if we removed the last one then fix up end
					end = null;
			}
		}
	}
	
	void Draw(){
		for (Particle temp = start; temp != null; temp = temp.next){
			Quake.drawParticle(temp.org[0], temp.org[1], temp.org[2], 5, 1, 0, 0, 1, 0);
		}
	}
}
